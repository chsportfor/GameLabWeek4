#include "ObjViewer.h"

#include <stdexcept>
#include <vector>

#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/IO/FileManager.h"
#include "Editor/Console.h"
#include "Editor/FEditorViewportClient.h"
#include "Engine/Assets/InitializeAssets.h"
#include "Platform/WindowApplication.h"
#include "Rendering/RenderInfo.h"
#include "Rendering/Renderer.h"
#include "ThirdParty/ImGui/imgui.h"

FObjViewer::FObjViewer(UAssetManager& InAssetManager, URenderer& InRenderer,
	FFileManager& InFileManager, FEditorViewportClient& InViewportClient)
	: AssetManager(InAssetManager)
	, Renderer(InRenderer)
	, FileManager(InFileManager)
	, ViewportClient(InViewportClient)
{
}

void FObjViewer::DrawControls()
{
	if (ImGui::Button("Open OBJ..."))
	{
		OpenObjFileDialog();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("Ctrl+O");

	if (!ImGui::GetIO().WantCaptureKeyboard
		&& WindowApplication.Input.IsDown(VK_CONTROL)
		&& WindowApplication.Input.WasPressed('O'))
	{
		OpenObjFileDialog();
	}

	ImGui::Separator();
	if (Mesh)
	{
		ImGui::TextUnformatted("Loaded file:");
		ImGui::TextWrapped("%s", Path.CStr());
		ImGui::Text("Vertices: %u", VertexCount);
		ImGui::Text("Triangles: %u", TriangleCount);
		ImGui::Text("Sections: %u", SectionCount);
		ImGui::Text("Materials: %u", MaterialCount);
	}
	else
	{
		ImGui::TextDisabled("Select an OBJ file to begin.");
	}

	ImGui::Separator();
	ImGui::TextDisabled("MTL diffuse colors and textures are supported.");
	ImGui::TextDisabled("Left mouse: rotate model  |  Right mouse: look");
	ImGui::TextDisabled("WASDQE: move camera  |  Wheel: zoom");

	if (Error.Len() > 0)
	{
		ImGui::Separator();
		ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Load failed:");
		ImGui::TextWrapped("%s", Error.CStr());
	}
}

void FObjViewer::UpdateControls(float DeltaTime, float PerspectiveRatio,
	bool bAllowMouseInput, bool bAllowKeyboardInput)
{
	ViewportClient.UpdateCameraControls(DeltaTime, PerspectiveRatio,
		bAllowMouseInput, bAllowKeyboardInput);
	UpdateModelControls(bAllowMouseInput);
}

void FObjViewer::UpdateModelControls(bool bAllowMouseInput)
{
	if (!Mesh || !bAllowMouseInput
		|| !WindowApplication.Input.IsDown(VK_LBUTTON))
	{
		return;
	}

	constexpr float RotationSensitivity = 0.25f;
	Rotation.Yaw = FMath::Fmod(
		Rotation.Yaw - WindowApplication.Input.MouseDX * RotationSensitivity, 360.0f);
	Rotation.Pitch = FMath::Fmod(
		Rotation.Pitch - WindowApplication.Input.MouseDY * RotationSensitivity, 360.0f);
}

void FObjViewer::SubmitRenderInfos(FRenderCollector& Collector) const
{
	if (!Mesh)
	{
		return;
	}

	const FMatrix ModelTransform = FMatrix::Translation(-Center)
		* FMatrix::Rotate(Rotation)
		* FMatrix::Translation(Center);
	const TArray<FMeshSection>& Sections = Mesh->GetSections();
	const TArray<UMaterial*>& Materials = Mesh->GetMaterials();
	for (const FMeshSection& Section : Sections)
	{
		if (Section.MaterialIndex >= static_cast<uint32>(Materials.Num()))
		{
			continue;
		}

		const UMaterial* Material = Materials[Section.MaterialIndex];
		if (!Material)
		{
			continue;
		}

		FRenderMeshInfo MeshInfo{};
		MeshInfo.StaticMesh = Mesh;
		MeshInfo.Texture = Material->DiffuseTexture;
		MeshInfo.WorldTransformMatrix = ModelTransform;
		MeshInfo.Color = Material->DiffuseColor;
		MeshInfo.FirstIndex = Section.FirstIndex;
		MeshInfo.IndexCount = Section.IndexCount;
		Collector.MeshInfos.Add(MeshInfo);
	}
}

bool FObjViewer::LoadObjFile(const std::filesystem::path& FilePath)
{
	const FString DisplayPath = Wide2Utf(FilePath.wstring());
	try
	{
		const FName MeshName = RegisterObjFileAsset(FilePath, AssetManager, Renderer, FileManager);
		UStaticMeshAsset* LoadedMesh = AssetManager.GetAssetAs<UStaticMeshAsset>(MeshName, true);
		if (!LoadedMesh)
		{
			throw std::runtime_error("Failed to create the OBJ GPU mesh.");
		}

		Mesh = LoadedMesh;
		Path = DisplayPath;
		Error.Reset();
		VertexCount = Mesh->GetVertexCount();
		TriangleCount = Mesh->GetIndexCount() / 3;
		SectionCount = static_cast<uint32>(Mesh->GetSections().Num());
		MaterialCount = static_cast<uint32>(Mesh->GetMaterials().Num());
		Center = (Mesh->GetLocalBoundingBox().Min + Mesh->GetLocalBoundingBox().Max) * 0.5f;
		Rotation = FRotator(0.0f, 0.0f, 0.0f);
		FrameCamera(Mesh->GetLocalBoundingBox());
		UE_LOG_F(Log, Core, "Loaded OBJ '{}': {} vertices, {} triangles.", DisplayPath,
			VertexCount, TriangleCount);
		return true;
	}
	catch (const std::exception& Exception)
	{
		Error = std::string_view(Exception.what());
		UE_LOG_F(Error, Core, "Failed to upload OBJ '{}': {}", DisplayPath, Exception.what());
		return false;
	}
}

void FObjViewer::Reset()
{
	Mesh = nullptr;
}

void FObjViewer::OpenObjFileDialog()
{
	std::vector<wchar_t> FileName(32768, L'\0');
	OPENFILENAMEW OpenFileName{};
	OpenFileName.lStructSize = sizeof(OpenFileName);
	OpenFileName.hwndOwner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
	OpenFileName.lpstrFilter = L"OBJ Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
	OpenFileName.lpstrFile = FileName.data();
	OpenFileName.nMaxFile = static_cast<DWORD>(FileName.size());
	OpenFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	OpenFileName.lpstrDefExt = L"obj";

	if (GetOpenFileNameW(&OpenFileName))
	{
		LoadObjFile(std::filesystem::path(FileName.data()));
	}
}

void FObjViewer::FrameCamera(const FBoundingBox& Bounds)
{
	const FVector BoundsCenter = (Bounds.Min + Bounds.Max) * 0.5f;
	const float Radius = FMath::Max((Bounds.Max - Bounds.Min).Length() * 0.5f, 0.5f);
	FCamera& Camera = ViewportClient.GetCamera();
	Camera.Location = BoundsCenter + FVector(-Radius * 2.5f, -Radius * 2.5f, Radius * 1.5f);
	Camera.LookAt(BoundsCenter);
	Camera.Velocity = FVector(0.f);
	Camera.mOrthoDistance = Radius * 2.5f;
	Camera.mFarPlane = FMath::Max(Radius * 6.0f, FCamera::FarPlane);
}
