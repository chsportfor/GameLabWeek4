#include "ObjViewer.h"

#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "Core/AssetSystem/Asset/Texture2DAsset.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/IO/FileManager.h"
#include "Core/Object/ObjectFactory.h"
#include "Editor/Console.h"
#include "Editor/FEditorViewportClient.h"
#include "Engine/Assets/InitializeAssets.h"
#include "Engine/Assets/ObjImporter.h"
#include "Engine/Assets/StaticMesh.h"
#include "Platform/WindowApplication.h"
#include "Rendering/RenderInfo.h"
#include "Rendering/Renderer.h"
#include "Rendering/VertexType.h"
#include "ThirdParty/ImGui/imgui.h"

FObjViewer::FObjViewer(UAssetManager& InAssetManager, URenderer& InRenderer,
	FFileManager& InFileManager, FEditorViewportClient& InViewportClient)
	: AssetManager(InAssetManager)
	, Renderer(InRenderer)
	, FileManager(InFileManager)
	, ViewportClient(InViewportClient)
{
}

FObjViewer::~FObjViewer()
{
	ReleasePreview();
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
	if (PreviewMesh)
	{
		ImGui::TextUnformatted("Loaded file:");
		ImGui::TextWrapped("%s", Path.CStr());
		ImGui::Text("Vertices: %u", VertexCount);
		ImGui::Text("Triangles: %u", TriangleCount);
		ImGui::Text("Sections: %u", SectionCount);
		ImGui::Text("Materials: %u", MaterialCount);
		if (ImGui::Checkbox("Flip texture V", &bFlipTextureV))
		{
			ImportedAssetName.Reset();
		}
		ImGui::SameLine();
		if (ImGui::Button("Import selected UV"))
		{
			ImportPreview();
		}
		if (ImportedAssetName.Len() > 0)
		{
			ImGui::Text("Imported asset: %s", ImportedAssetName.CStr());
		}
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
	if (!PreviewMesh || !bAllowMouseInput
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
	if (!PreviewMesh || !PreviewVertexBuffer)
	{
		return;
	}

	const FMatrix ModelTransform = FMatrix::Translation(-Center)
		* FMatrix::Rotate(Rotation)
		* FMatrix::Translation(Center);
	for (const FStaticMeshSection& Section : PreviewMesh->Sections)
	{
		if (Section.MaterialIndex >= static_cast<uint32>(PreviewMesh->Materials.Num()))
		{
			continue;
		}

		const FStaticMaterial& Material = PreviewMesh->Materials[Section.MaterialIndex];
		FRenderStaticMeshInfo MeshInfo{};
		MeshInfo.VertexBuffer = PreviewVertexBuffer;
		MeshInfo.IndexBuffer = PreviewIndexBuffer;
		MeshInfo.VertexCount = static_cast<uint32>(PreviewMesh->Vertices.Num());
		MeshInfo.Texture = Section.MaterialIndex < static_cast<uint32>(PreviewMaterialTextures.Num())
			? PreviewMaterialTextures[Section.MaterialIndex] : nullptr;
		MeshInfo.WorldTransformMatrix = ModelTransform;
		MeshInfo.Color = MeshInfo.Texture
			? FLinearColor(1.0f, 1.0f, 1.0f, Material.DiffuseColor.w)
			: FLinearColor(Material.DiffuseColor.x, Material.DiffuseColor.y,
				Material.DiffuseColor.z, Material.DiffuseColor.w);
		if (bFlipTextureV)
		{
			MeshInfo.UVScale = FVector2(1.0f, -1.0f);
			MeshInfo.UVOffset = FVector2(0.0f, 1.0f);
		}
		MeshInfo.FirstIndex = Section.FirstIndex;
		MeshInfo.IndexCount = Section.NumIndices;
		Collector.StaticMeshInfos.Add(MeshInfo);
	}
}

bool FObjViewer::LoadObjFile(const std::filesystem::path& FilePath)
{
	const FString DisplayPath = Wide2Utf(FilePath.wstring());
	ReleasePreview();
	try
	{
		auto ParsedMesh = std::make_unique<FStaticMesh>();
		FString ParseError;
		if (!FObjImporter::LoadFromFile(FilePath, FileManager, *ParsedMesh, ParseError))
		{
			throw std::runtime_error(ParseError.CStr());
		}

		TArray<FVertexSimple> Vertices;
		Vertices.Reserve(ParsedMesh->Vertices.Num());
		for (const FVertexPNCT& Vertex : ParsedMesh->Vertices)
		{
			Vertices.Add({Vertex.Position.x, Vertex.Position.y, Vertex.Position.z,
				Vertex.Normal.x, Vertex.Normal.y, Vertex.Normal.z,
				Vertex.Color.x, Vertex.Color.y, Vertex.Color.z, Vertex.Color.w,
				Vertex.UV.x, Vertex.UV.y});
		}
		PreviewVertexBuffer = Renderer.CreateVertexBuffer(Vertices.GetData(), Vertices.Num());
		PreviewIndexBuffer = Renderer.CreateIndexBuffer(
			ParsedMesh->Indices.GetData(), ParsedMesh->Indices.Num());
		if (!PreviewVertexBuffer || !PreviewIndexBuffer)
		{
			throw std::runtime_error("Failed to create the OBJ preview GPU mesh.");
		}

		PreviewMaterialTextures.SetNum(ParsedMesh->Materials.Num());
		std::unordered_map<std::wstring, UTexture2D*> LoadedTextures;
		for (int32 MaterialIndex = 0; MaterialIndex < ParsedMesh->Materials.Num(); ++MaterialIndex)
		{
			const FString& TexturePath = ParsedMesh->Materials[MaterialIndex].DiffuseTexturePath;
			if (TexturePath.Len() == 0) continue;
			try
			{
				const auto ResolvedPath = FileManager.ResolvePath(
					std::filesystem::path(Utf2Wide(TexturePath)));
				if (const auto Found = LoadedTextures.find(ResolvedPath.wstring()); Found != LoadedTextures.end())
				{
					PreviewMaterialTextures[MaterialIndex] = Found->second;
					continue;
				}
				const FString Bytes = FileManager.ReadFileToString(ResolvedPath);
				auto Texture = Renderer.CreateTexture2DFromMemory(Bytes.CStr(), Bytes.Len());
				if (!Texture) throw std::runtime_error("texture GPU upload failed");
				auto View = Renderer.CreateShaderResourceView(Texture);
				if (!View) throw std::runtime_error("texture view creation failed");
				auto* PreviewTexture = FObjectFactory::ConstructObject<UTexture2D>(
					std::move(Texture), std::move(View));
				if (!PreviewTexture) throw std::runtime_error("texture object creation failed");
				OwnedPreviewTextures.Add(PreviewTexture);
				PreviewMaterialTextures[MaterialIndex] = PreviewTexture;
				LoadedTextures.emplace(ResolvedPath.wstring(), PreviewTexture);
			}
			catch (const std::exception& TextureError)
			{
				UE_LOG(Warning, Core, "OBJ preview skipped texture: %s", TextureError.what());
			}
		}

		FBoundingBox Bounds(ParsedMesh->Vertices[0].Position, ParsedMesh->Vertices[0].Position);
		for (const FVertexPNCT& Vertex : ParsedMesh->Vertices) Bounds.ExpandToInclude(Vertex.Position);
		PreviewMesh = ParsedMesh.release();
		SourcePath = FilePath;
		Path = DisplayPath;
		Error.Reset();
		ImportedAssetName.Reset();
		VertexCount = static_cast<uint32>(PreviewMesh->Vertices.Num());
		TriangleCount = static_cast<uint32>(PreviewMesh->Indices.Num()) / 3;
		SectionCount = static_cast<uint32>(PreviewMesh->Sections.Num());
		MaterialCount = static_cast<uint32>(PreviewMesh->Materials.Num());
		bFlipTextureV = false;
		Center = (Bounds.Min + Bounds.Max) * 0.5f;
		Rotation = FRotator(0.0f, 0.0f, 0.0f);
		FrameCamera(Bounds);
		UE_LOG_F(Log, Core, "Previewed OBJ '{}': {} vertices, {} triangles.", DisplayPath,
			VertexCount, TriangleCount);
		return true;
	}
	catch (const std::exception& Exception)
	{
		ReleasePreview();
		Path = DisplayPath;
		Error = std::string_view(Exception.what());
		UE_LOG_F(Error, Core, "Failed to preview OBJ '{}': {}", DisplayPath, Exception.what());
		return false;
	}
}

void FObjViewer::ImportPreview()
{
	if (!PreviewMesh) return;
	try
	{
		const FName AssetName = ImportStaticMeshAsset(*PreviewMesh, SourcePath,
			bFlipTextureV, AssetManager, Renderer, FileManager);
		ImportedAssetName = AssetName.ToString();
		Error.Reset();
		UE_LOG_F(Log, Core, "Imported OBJ preview as '{}'.", ImportedAssetName);
	}
	catch (const std::exception& Exception)
	{
		Error = std::string_view(Exception.what());
		UE_LOG_F(Error, Core, "Failed to import OBJ preview '{}': {}", Path, Exception.what());
	}
}

void FObjViewer::ReleasePreview()
{
	delete PreviewMesh;
	PreviewMesh = nullptr;
	PreviewVertexBuffer.Reset();
	PreviewIndexBuffer.Reset();
	for (UTexture2D* Texture : OwnedPreviewTextures)
	{
		if (Texture) Texture->Destroy();
	}
	OwnedPreviewTextures.Reset();
	PreviewMaterialTextures.Reset();
	SourcePath.clear();
}

void FObjViewer::Reset()
{
	ReleasePreview();
	Path.Reset();
	Error.Reset();
	ImportedAssetName.Reset();
	VertexCount = 0;
	TriangleCount = 0;
	SectionCount = 0;
	MaterialCount = 0;
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
