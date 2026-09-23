#include "EditorUIManager.h"

#include "AssetDragDrop.h"
#include "ThirdParty/ImGui/imgui.h"
#include "ThirdParty/ImGui/imgui_internal.h"
#include "ThirdParty/ImGui/imgui_impl_dx11.h"
#include "ThirdParty/ImGui/imgui_impl_win32.h"

#include "Core/FrameTimer.h"
#include "Core/IO/FileManager.h"
#include "Rendering/RenderingPipeline.h"
#include "Rendering/BuiltinAssetNames.h"
#include "Engine/SceneManager.h"
#include "Engine/Components/ActorComponent.h"
#include "Engine/Components/PrimitiveComponent.h"
#include "Engine/Components/UStaticMeshComponent.h"
#include "Engine/Components/ParticleSubUVComponent.h"
#include "Core/Object/Objectiterator.h"

/* Editor */
#include "FEditorViewportClient.h"
#include "Console.h"
#include "OverlayStat.h"
#include "Platform/WindowApplication.h"
#include <algorithm>


void FEditorUIManager::LoadSettings(FEditorCommands& outCommands)
{
	mEditorSetting.Load();

	// Load settings into commands
	outCommands.Emplace(FSetCameraSensitivityCommand{ mEditorSetting.CameraSensitivity });
	outCommands.Emplace(FSetGridWidthCommand{ mEditorSetting.GridSpacing });
	outCommands.Emplace(FSetRatioHCommand{ mEditorSetting.RatioH });
	outCommands.Emplace(FSetRatioVCommand{ mEditorSetting.RatioV });
	//outCommands.Emplace(FSetCameraLocationCommand{ mEditorSetting.CameraLocation});
	//outCommands.Emplace(FSetCameraRotationCommand{ mEditorSetting.CameraRotation });
	//outCommands.Emplace(FSetCameraFovCommand{ mEditorSetting.CameraFOV });
}

void FEditorUIManager::SaveSettings(float ratioV, float ratioH)
{
	mEditorSetting.RatioH = ratioH;
	mEditorSetting.RatioV = ratioV;
	mEditorSetting.Save();
}


void FEditorUIManager::UpdateGui(const FGuiReference& guiReference, FEditorCommands& outCommands)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	updateDockSpace();
    const auto SelectionRevision = guiReference.SceneManager.GetSelectionRevision();
    if (SelectionRevision != mLastActorSelectionRevision) mPropertyTarget = EPropertyTarget::Actor;
    mLastActorSelectionRevision = SelectionRevision;

	updateControlPanelGUI(guiReference, outCommands);
	updateObjectListPanelGUI(guiReference, outCommands);

	ConsoleWindow::Get().Draw();
	// Also attach the new browser to the console dock in layouts saved before it existed.
	if (const ImGuiWindow* Console = ImGui::FindWindowByName("Jungle Console Window"); Console && Console->DockId)
		ImGui::SetNextWindowDockID(Console->DockId, ImGuiCond_FirstUseEver);
	mContentBrowser.Draw(guiReference.FileManager, guiReference.AssetManager, *guiReference.RenderingPipeline.GetRenderer());
    if (const auto Folder = mContentBrowser.ConsumeStaticMeshImport())
        outCommands.Emplace(FOpenStaticMeshImportCommand{Wide2Utf(Folder->wstring())});
    const auto ClickedAsset = mContentBrowser.ConsumeAssetClick();
    if (!ClickedAsset.empty())
    {
        mAssetProperties.Inspect(ClickedAsset, guiReference.FileManager);
        mPropertyTarget = EPropertyTarget::Asset;
    }
    updatePropertyWindowGUI(guiReference, outCommands);
	OverlayStatWindow::GetInstance().SetStats(guiReference);
	OverlayStatWindow::GetInstance().DrawStat(mSceneViewportRect);

    // WEEK3 keys; route through the same command as the button and preserve Ctrl+V paste.
    const auto& Input = WindowApplication.Input;
    if (!ImGui::GetIO().WantCaptureKeyboard && !Input.IsDown(VK_CONTROL)
        && !Input.IsDown(VK_MENU) && !Input.IsDown(VK_SHIFT))
    {
        if (Input.WasPressed(VK_SPACE)) outCommands.Emplace(FCycleGizmoModeCommand{});
        if (Input.WasPressed('V')) outCommands.Emplace(FSetGizmoWorldModeCommand{true});
        else if (Input.WasPressed('B')) outCommands.Emplace(FSetGizmoWorldModeCommand{false});
    }
}

void FEditorUIManager::updateDockSpace()
{
    const ImGuiViewport* Viewport = ImGui::GetMainViewport();
    const ImGuiID DockspaceID = ImGui::GetID("EditorDockSpace");
    // Keep an empty, transparent center for the existing D3D11 split viewports.
    const ImGuiDockNodeFlags Flags = ImGuiDockNodeFlags_PassthruCentralNode
        | ImGuiDockNodeFlags_NoDockingOverCentralNode;
    if (!ImGui::DockBuilderGetNode(DockspaceID))
    {
        ImGui::DockBuilderAddNode(DockspaceID, ImGuiDockNodeFlags_DockSpace | Flags);
        ImGui::DockBuilderSetNodePos(DockspaceID, Viewport->WorkPos);
        ImGui::DockBuilderSetNodeSize(DockspaceID, Viewport->WorkSize);
        ImGuiID Center = DockspaceID, Left, Bottom, Control, Properties, Objects;
        ImGui::DockBuilderSplitNode(Center, ImGuiDir_Left, 0.25f, &Left, &Center);
        ImGui::DockBuilderSplitNode(Center, ImGuiDir_Down, 0.25f, &Bottom, &Center);
        ImGui::DockBuilderSplitNode(Left, ImGuiDir_Up, 0.45f, &Control, &Left);
        ImGui::DockBuilderSplitNode(Left, ImGuiDir_Up, 0.55f, &Properties, &Objects);
        ImGui::DockBuilderDockWindow("PODO", Control);
        ImGui::DockBuilderDockWindow("Jungle Property Window", Properties);
        ImGui::DockBuilderDockWindow("Object List Panel", Objects);
        ImGui::DockBuilderDockWindow("Jungle Console Window", Bottom);
        ImGui::DockBuilderDockWindow("Content Browser", Bottom);
        ImGui::DockBuilderFinish(DockspaceID);
    }
    ImGui::DockSpaceOverViewport(DockspaceID, Viewport, Flags);
    mSceneViewportRect = {};
    if (const ImGuiDockNode* Center = ImGui::DockBuilderGetCentralNode(DockspaceID))
    {
        // Input and D3D11 use client coordinates. Platform viewports remain disabled.
        mSceneViewportRect = {Center->Pos.x - Viewport->Pos.x, Center->Pos.y - Viewport->Pos.y,
            Center->Size.x, Center->Size.y};
    }
}

FString saveSceneFileDialog();
FString openSceneFileDialog();
FString openObjFileDialog();

void FEditorUIManager::updateControlPanelGUI(const FGuiReference& guiReference, FEditorCommands& outCommands)
{
	ImGui::SetNextWindowSize(ImVec2(340, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("PODO", nullptr, ImGuiWindowFlags_NoCollapse);

	/* Spawn Actor */
	ImGui::SeparatorText("Spawn Actor");

	// Spawn presets select a display name and a mesh asset; all use AStaticMeshActor.
	const char* actorTypeNames[] = { "Sphere", "Cube", "Triangle", "StaticMesh" };
	const char* meshNames[] = { BuiltinAssetNames::SphereMesh, BuiltinAssetNames::CubeMesh,
        BuiltinAssetNames::TriangleMesh, BuiltinAssetNames::CubeMesh };
    int32 spawnCount = mGuiInputField.SpawnCount;

	ImGui::Combo("Actor Type", &mGuiInputField.SpawnTypeIndex, actorTypeNames, IM_ARRAYSIZE(actorTypeNames));
	if (ImGui::Button("Spawn"))
	{
        const int32 Index = mGuiInputField.SpawnTypeIndex;
        outCommands.Emplace(FSpawnStaticMeshActorCommand{
            actorTypeNames[Index], meshNames[Index], mGuiInputField.SpawnCount });
	}
	ImGui::SameLine();
	if (ImGui::InputInt("Number of spawn", &spawnCount))
	{
		if (spawnCount < 1)
		{
			spawnCount = 1;
		}
		mGuiInputField.SpawnCount = spawnCount;
	}
	if (ImGui::Button("Spawn Particle"))
	{
		outCommands.Emplace(FSpawnParticleCommand{});
	}

	ImGui::SeparatorText("Asset Import");
	
	if (ImGui::Button("Import StaticMesh"))
	{
		outCommands.Emplace(FOpenStaticMeshImportCommand{Wide2Utf(mContentBrowser.GetCurrentFolder().wstring())});
	}

	/* Scene Control */
	ImGui::SeparatorText("Scene Control");

	ImGui::InputText("Scene Name", mGuiInputField.SceneName, IM_ARRAYSIZE(mGuiInputField.SceneName), ImGuiInputTextFlags_ReadOnly);
	if (ImGui::Button("New scene"))
	{
		//guiReference.ViewportClient->Reset();
		//NewScene();
		outCommands.Emplace(FNewSceneCommand{});
		strcpy_s(mGuiInputField.SceneName, sizeof(mGuiInputField.SceneName), "Default");
	}
	ImGui::SameLine();
	if (ImGui::Button("Save scene"))
	{
		const FString selectedFile = saveSceneFileDialog();

		if (selectedFile.Len() > 0)
		{
			const std::filesystem::path selectedPath(selectedFile.CStr());
			const FString sceneName(selectedPath.stem().string());

			outCommands.Emplace(FSaveSceneCommand{ sceneName });
			strcpy_s(
				mGuiInputField.SceneName,
				sizeof(mGuiInputField.SceneName),
				sceneName.CStr());
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Load scene"))
	{
		const FString selectedFile = openSceneFileDialog();

		if (selectedFile.Len() > 0)
		{
			//LoadScene(selectedFile, *guiReference.FileManager);
			//std::filesystem::path p(selectedFile.CStr());
			//std::string LoadScenename = p.stem().string();
			//strcpy_s(mGuiInputField.SceneName, sizeof(mGuiInputField.SceneName), LoadScenename.c_str());
			//guiReference.ViewportClient->Reset();
			outCommands.Emplace(FLoadSceneCommand{ selectedFile });
		}
	}

	//const FCamera& camera = guiReference.ViewportClient.GetCamera();

	if (ImGui::BeginCombo("##ShowFlags", "Show Flags"))
	{
		uint32 showFlags = guiReference.RenderingPipeline.GetShowFlags();
		bool bShowFlagsChanged = false;

		bool bPrimitives = showFlags & static_cast<uint32>(EEngineShowFlags::SF_Primitives);
		if (ImGui::Checkbox("Primitives", &bPrimitives))
		{

			bShowFlagsChanged = true;
		}

		bool bBillboardText = showFlags & static_cast<uint32>(EEngineShowFlags::SF_BillboardText);
		if (ImGui::Checkbox("Billboard Text", &bBillboardText))
		{

			bShowFlagsChanged = true;
		}

		bool bShowWorldAxis = showFlags & static_cast<uint32>(EEngineShowFlags::SF_WorldAxis);
		if (ImGui::Checkbox("World axis", &bShowWorldAxis))
		{

			bShowFlagsChanged = true;
		}

		bool bShowBoundingBox = showFlags & static_cast<uint32>(EEngineShowFlags::SF_BoundingBox);
		if (ImGui::Checkbox("Bounding Box", &bShowBoundingBox))
		{
			bShowFlagsChanged = true;
		}

		bool bShowGrid = showFlags & static_cast<uint32>(EEngineShowFlags::SF_Grid);
		if (ImGui::Checkbox("Grid", &bShowGrid))
		{
			bShowFlagsChanged = true;
		}

		// Set the show flags based on the checkbox values
		if (bShowFlagsChanged)
		{
			showFlags = 0;
			showFlags += bPrimitives ? static_cast<uint32>(EEngineShowFlags::SF_Primitives) : 0;
			showFlags += bBillboardText ? static_cast<uint32>(EEngineShowFlags::SF_BillboardText) : 0;
			showFlags += bShowWorldAxis ? static_cast<uint32>(EEngineShowFlags::SF_WorldAxis) : 0;
			showFlags += bShowBoundingBox ? static_cast<uint32>(EEngineShowFlags::SF_BoundingBox) : 0;
			showFlags += bShowGrid ? static_cast<uint32>(EEngineShowFlags::SF_Grid) : 0;

			outCommands.Emplace(FSetShowFlagCommand{ showFlags });
		}
		ImGui::EndCombo();
	}

	bool bOrthographic = guiReference.RenderingPipeline.IsOrthographicTarget();
	if (ImGui::Checkbox("Orthogonal", &bOrthographic))
	{

		//{
		//	const FVector offset = selectedActor->GetTransform().Location - camera.Location;
		//	const float depth = FVector::dot(offset, camera.GetForwardVector());
		//	camera.mOrthoDistance = FMath::Max(depth, 0.1f);
		//}

		outCommands.Emplace(FStartProjectionTransitionCommand{ bOrthographic });
	}

	/* Camera Control */
	ImGui::SeparatorText("Camera Control");

	const FCamera& camera = guiReference.ViewportClient.GetCamera();
	float cameraFov = camera.mFovDegree;
	float cameraLocation[3] = { camera.Location.x, camera.Location.y, camera.Location.z };
	float cameraRotation[3] = { camera.Rotation.Roll, camera.Rotation.Pitch, camera.Rotation.Yaw };
	float cameraSensitivity = camera.Sensitivity;
	bool bCameraLocationChanged = false;
	bool bCameraRotationChanged = false;

	ImGui::Text("FOV      ");
	ImGui::SameLine();
	if (ImGui::SliderFloat("##FOV", &cameraFov, 0.0f, 180.0f))
	{
		outCommands.Emplace(FSetCameraFovCommand{ cameraFov });
	}

	// 1) 라벨 텍스트를 먼저 그리고 같은 줄로
	ImGui::Text("Location ");
	ImGui::SameLine();

	// 2) 텍스트를 그린 "뒤"의 남은 폭을 기준으로 계산
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float itemWidth = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f;

	ImGui::SetNextItemWidth(itemWidth);
	if (ImGui::DragFloat("##CamLocX", &cameraLocation[0], 0.1f, 10.0f))
	{
		bCameraLocationChanged = true;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(itemWidth);
	if (ImGui::DragFloat("##CamLocY", &cameraLocation[1], 0.1f, 10.0f))
	{
		bCameraLocationChanged = true;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(itemWidth);
	if (ImGui::DragFloat("##CamLocZ", &cameraLocation[2], 0.1f, 10.0f))
	{
		bCameraLocationChanged = true;
	}

	ImGui::Text("Rotation ");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(itemWidth);
	if (ImGui::DragFloat("##CamRotX", &cameraRotation[0], 0.1f, 180.0f))
	{
		bCameraRotationChanged = true;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(itemWidth);
	if (ImGui::DragFloat("##CamRotY", &cameraRotation[1], 0.1f, 180.0f))
	{
		bCameraRotationChanged = true;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(itemWidth);
	if (ImGui::DragFloat("##CamRotZ", &cameraRotation[2], 0.1f, 180.0f))
	{
		bCameraRotationChanged = true;
	}

	if (bCameraLocationChanged)
	{
		outCommands.Emplace(FSetCameraLocationCommand{ FVector{ cameraLocation[0], cameraLocation[1], cameraLocation[2] } });
	}

	if (bCameraRotationChanged)
	{
		outCommands.Emplace(FSetCameraRotationCommand{ FRotator{ cameraRotation[1], cameraRotation[2], cameraRotation[0] } });
	}

	{
		static constexpr int32 GridGapValues[] = { 1, 5, 10, 50, 100, 500 };
		static constexpr const char* GridGapLabels[] = { "(1)", "(5)", "(10)", "(50)", "(100)", "(500)" };
		constexpr int StepCount = IM_ARRAYSIZE(GridGapValues);
		const float GridGap = guiReference.RenderingPipeline.GetGridWidth();
		int SelectedIndex = 0;
		for (int i = 1; i < StepCount; ++i)
		{
			if (GridGap >= (GridGapValues[i - 1] + GridGapValues[i]) / 2.0f)
			{
				SelectedIndex = i;
			}
		}

		ImGui::Text("Grid Gap: %g", GridGap);
		const ImGuiStyle& Style = ImGui::GetStyle();
		const float FontSize = ImGui::GetFontSize();
		const float LabelWidth = ImGui::CalcTextSize("(500)").x;
		const float Width = (std::max)(ImGui::GetContentRegionAvail().x,
			(LabelWidth + Style.ItemInnerSpacing.x) * StepCount);
		const float Padding = LabelWidth * 0.5f;
		const ImVec2 Origin = ImGui::GetCursorScreenPos();
		const float TrackLeft = Origin.x + Padding;
		const float TrackWidth = Width - Padding * 2.0f;
		const float TrackY = Origin.y + FontSize;
		const float TrackHeight = FontSize * 0.3f;
		const float LabelY = TrackY + TrackHeight + Style.ItemInnerSpacing.y;
		ImGui::InvisibleButton("##GridGapSelector",
			ImVec2(Width, LabelY + FontSize - Origin.y));
		const bool bActive = ImGui::IsItemActive();
		const bool bHovered = ImGui::IsItemHovered();
		bool bChanged = false;
		if (bActive && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			// Snap to the closest displayed step, including when dragging past either end.
			const float Position = std::clamp((ImGui::GetIO().MousePos.x - TrackLeft) / TrackWidth, 0.0f, 1.0f);
			SelectedIndex = static_cast<int>(Position * (StepCount - 1) + 0.5f);
			bChanged = true;
		}
		if (bChanged && GridGapValues[SelectedIndex] != GridGap)
		{
			const float NewGap = static_cast<float>(GridGapValues[SelectedIndex]);
			mEditorSetting.GridSpacing = NewGap;
			outCommands.Emplace(FSetGridWidthCommand{ NewGap });
		}

		if (ImGui::IsItemVisible())
		{
			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			const ImU32 TrackColor = ImGui::GetColorU32(bActive ? ImGuiCol_FrameBgActive :
				(bHovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg));
			const ImU32 HandleColor = ImGui::GetColorU32(bActive ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);
			DrawList->AddRectFilled(ImVec2(TrackLeft, TrackY),
				ImVec2(TrackLeft + TrackWidth, TrackY + TrackHeight), TrackColor, Style.FrameRounding);
			for (int i = 0; i < StepCount; ++i)
			{
				const float X = TrackLeft + TrackWidth * i / (StepCount - 1);
				const ImU32 LabelColor = ImGui::GetColorU32(i == SelectedIndex ? ImGuiCol_Text : ImGuiCol_TextDisabled);
				DrawList->AddLine(ImVec2(X, TrackY), ImVec2(X, TrackY + TrackHeight), LabelColor);
				DrawList->AddText(ImVec2(X - ImGui::CalcTextSize(GridGapLabels[i]).x * 0.5f, LabelY),
					LabelColor, GridGapLabels[i]);
			}
			const float HandleX = TrackLeft + TrackWidth * SelectedIndex / (StepCount - 1);
			DrawList->AddTriangleFilled(ImVec2(HandleX - FontSize * 0.4f, Origin.y),
				ImVec2(HandleX + FontSize * 0.4f, Origin.y), ImVec2(HandleX, TrackY + TrackHeight), HandleColor);
		}
	}

	ImGui::Text("Sensitivity");
	ImGui::SameLine();
	if (ImGui::SliderFloat("##Sensitivity", &cameraSensitivity, 0.0f, 1.0f))
	{
		outCommands.Emplace(FSetCameraSensitivityCommand{ cameraSensitivity });
	}

	/* Gizmo Control */
	ImGui::SeparatorText("Gizmo Control");

    const bool bWorldMode = guiReference.ViewportClient.mGizmo.IsWorldMode();
    if (ImGui::Button(bWorldMode ? "Space: Global##GizmoSpace" : "Space: Local##GizmoSpace"))
        outCommands.Emplace(FSetGizmoWorldModeCommand{!bWorldMode});
    ImGui::SameLine();
    ImGui::TextDisabled("V: Global / B: Local");

	// Display the current gizmo mode dropdown
	const char* gizmoModeNames[] = { "Translate", "Rotate", "Scale" };
	int32 gizmoModeIndex = static_cast<int32>(guiReference.ViewportClient.mGizmo.eType);
	if (ImGui::Combo("Gizmo Mode", &gizmoModeIndex, gizmoModeNames, IM_ARRAYSIZE(gizmoModeNames)))
	{
		//guiReference.ViewportClient->mGizmo.SetGizmoType(static_cast<EGIZMO_TYPE>(gizmoModeIndex));
		outCommands.Emplace(FSetGizmoModeCommand{ static_cast<EGIZMO_TYPE>(gizmoModeIndex) });
	}
	if (ImGui::Button("Next Gizmo Mode"))
	{
		//guiReference.ViewportClient->mGizmo.CycleGizmoType();
		outCommands.Emplace(FCycleGizmoModeCommand{});

	}
	ImGui::SameLine();
	ImGui::TextDisabled("Space");


	ImGui::End();
}

FString openSceneFileDialog()
{
	char fileName[MAX_PATH] = {};
	OPENFILENAMEA openFileName = {};

	openFileName.lStructSize = sizeof(OPENFILENAMEA);
	openFileName.hwndOwner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);  // main window

	openFileName.lpstrFilter = "Scene Files (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0";
	openFileName.lpstrFile = fileName;
	openFileName.nMaxFile = MAX_PATH;

	openFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	openFileName.lpstrDefExt = "Scene";

	std::filesystem::path initialDirectory = std::filesystem::absolute(std::filesystem::path("Assets") / "SceneData");

	if (!std::filesystem::exists(initialDirectory))
	{
		std::filesystem::create_directories(initialDirectory);
	}

	const std::string initialDirectoryString = initialDirectory.string();

	openFileName.lpstrInitialDir = initialDirectoryString.c_str();

	if (GetOpenFileNameA(&openFileName))
	{
		return FString(fileName);
	}

	return FString("");
}

FString openObjFileDialog()
{
	char fileName[MAX_PATH] = {};
	OPENFILENAMEA openFileName = {};

	openFileName.lStructSize = sizeof(OPENFILENAMEA);
	openFileName.hwndOwner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
	openFileName.lpstrFilter = "Wavefront OBJ (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
	openFileName.lpstrFile = fileName;
	openFileName.nMaxFile = MAX_PATH;
	openFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	openFileName.lpstrDefExt = "obj";

	if (GetOpenFileNameA(&openFileName)) return FString(fileName);
	return FString("");
}

FString saveSceneFileDialog()
{
	char fileName[MAX_PATH] = {};
	OPENFILENAMEA openFileName = {};

	openFileName.lStructSize = sizeof(OPENFILENAMEA);
	openFileName.hwndOwner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);  // main window

	openFileName.lpstrFilter = "Scene Files (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0";
	openFileName.lpstrFile = fileName;
	openFileName.nMaxFile = MAX_PATH;

	openFileName.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	openFileName.lpstrDefExt = "Scene";

	std::filesystem::path initialDirectory = std::filesystem::absolute(std::filesystem::path("Assets") / "SceneData");

	if (!std::filesystem::exists(initialDirectory))
	{
		std::filesystem::create_directories(initialDirectory);
	}

	const std::string initialDirectoryString = initialDirectory.string();

	openFileName.lpstrInitialDir = initialDirectoryString.c_str();

	if (GetSaveFileNameA(&openFileName))
	{
		return FString(fileName);
	}

	return FString("");
}

void FEditorUIManager::updatePropertyWindowGUI(const FGuiReference& guiReference, FEditorCommands& outCommands)
{
	ImGui::SetNextWindowSize(ImVec2(340, 300), ImGuiCond_FirstUseEver);
	ImGui::Begin("Jungle Property Window", nullptr, ImGuiWindowFlags_NoCollapse);
    if (mPropertyTarget == EPropertyTarget::Asset && !mAssetProperties.ValidateSelection(guiReference.AssetManager))
        mPropertyTarget = EPropertyTarget::None;
    if (mPropertyTarget == EPropertyTarget::None)
    {
        mGuiInputField.NameEditObject.Reset();
        ImGui::TextUnformatted("Select an actor or click an asset to view its properties.");
        ImGui::End();
        return;
    }
    if (mPropertyTarget == EPropertyTarget::Asset)
    {
        mGuiInputField.NameEditObject.Reset();
        mAssetProperties.Draw(guiReference.FileManager, guiReference.AssetManager);
        ImGui::End();
        return;
    }

	AActor* selectedActor = guiReference.SceneManager.GetSelectedActor();
	if (!selectedActor)
	{
		mGuiInputField.NameEditObject.Reset();
		ImGui::TextUnformatted("Select an actor or click an asset to view its properties.");
	}
	if (selectedActor)
	{
		ImGui::SeparatorText("Actor");
		const TWeakObjectPtr<AActor> editTarget(selectedActor);
		const FString currentName = selectedActor->GetName().ToString();
		if (mGuiInputField.NameEditObject != editTarget ||
			!mGuiInputField.NameEditOriginal.Equals(currentName))
		{
			mGuiInputField.NameEditObject = editTarget;
			mGuiInputField.NameEditOriginal = currentName;
			strncpy_s(mGuiInputField.ActorName, sizeof(mGuiInputField.ActorName), currentName.CStr(), _TRUNCATE);
		}

		ImGui::PushID(selectedActor);
		ImGui::TextUnformatted("Name");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Apply").x -
			ImGui::GetStyle().FramePadding.x * 2 - ImGui::GetStyle().ItemSpacing.x);
		const bool enterPressed = ImGui::InputText("##ActorName", mGuiInputField.ActorName,
			sizeof(mGuiInputField.ActorName), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		const bool applyPressed = ImGui::Button("Apply");
		if (enterPressed || applyPressed)
		{
			outCommands.Emplace(FSetActorNameCommand{ editTarget, FName(mGuiInputField.ActorName) });
			// Read back the accepted name after the command, including any assigned number.
			mGuiInputField.NameEditObject.Reset();
		}
		ImGui::PopID();

		ImGui::SeparatorText("Actor Transform");
		// Temporary variables to hold the values for ImGui input fields
		FTransform originalTransform = selectedActor->GetTransform();

		// Get the current transform of the clicked actor
		FVector translationInput = originalTransform.Location;
		FRotator originalRotator = selectedActor->GetRotator();
		float rotationInput[3] = {
			originalRotator.Roll,
			originalRotator.Pitch,
			originalRotator.Yaw
		};
		FVector scaleInput = originalTransform.Scale;

		// Display and edit the transform properties using ImGui input fields
		if (ImGui::DragFloat3("Translation", &translationInput.x, 0.1f))
		{
			//mSelectedActor->SetLocation(translationInput);
			outCommands.Emplace(FSetActorLocationCommand{ selectedActor, translationInput });
		}
		if (ImGui::DragFloat3("Rotation", &rotationInput[0], 0.1f))
		{
			//mSelectedActor->SetRotation(FRotator{
			//	rotationInput[1], // Pitch
			//	rotationInput[2], // Yaw
			//	rotationInput[0]  // Roll
			//	});

			outCommands.Emplace(FSetActorRotationCommand{ selectedActor, FRotator{
				rotationInput[1], // Pitch
				rotationInput[2], // Yaw
				rotationInput[0]  // Roll
				} });
		}
		if (ImGui::DragFloat3("Scale", &scaleInput.x, 0.1f, MIN_SCALE, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp))
		{
			//mSelectedActor->SetScale(scaleInput);
			outCommands.Emplace(FSetActorScaleCommand{ selectedActor, scaleInput });
		}

		/* Components */
		ImGui::SeparatorText("Components");
		if (ImGui::BeginChild("Components", ImVec2(0, 0), ImGuiChildFlags_Borders))
		{
			const TArray<UActorComponent*>& components = selectedActor->GetComponents();
			for (UActorComponent* component : components)
			{
				ImGui::PushID(component); // Ensure unique ID for each child
				if (ImGui::BeginChild("ComponentFrame", ImVec2(0, 0),
					ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY))
				{
					ImGui::Text("Class: %s", component->GetRuntimeClass()->Name.CStr());
					FString ComponentName = component->GetName().ToString();
					ImGui::Text("Name: %s | DisplayIndex: %d | ComparisonIndex: %d",
						ComponentName.CStr(),
						component->GetName().DisplayIndex,
						component->GetName().ComparisonIndex
					);
					if (auto* staticMeshComponent = component->Cast<UStaticMeshComponent>())
					{
						updateStaticMeshProperties(*staticMeshComponent, guiReference.AssetManager, outCommands);
					}
                    else if (auto* billboard = component->Cast<UBillboardComponent>())
                    {
                        FLinearColor color = billboard->GetColor();
                        if (ImGui::ColorEdit4("Color", &color.R))
                            outCommands.Emplace(FSetComponentColorCommand{ billboard, color });
                    }

					if (UParticleSubUVComponent* particleSubUVComponent =
						component->Cast<UParticleSubUVComponent>())
					{
						bool bLooping = particleSubUVComponent->IsLooping();
						float playRate = particleSubUVComponent->GetPlayRate();
						bool bUseAddtiveBlend = particleSubUVComponent->GetBlendStateType() == EBlendStateType::BST_Additive;

						if (ImGui::Checkbox("Looping", &bLooping))
						{
							outCommands.Emplace(FSetParticleSubUVComponentLoopingCommand{ particleSubUVComponent, bLooping });
						}
						if (ImGui::DragFloat("Play Rate", &playRate, 0.1f, 0.0f, 10.0f))
						{
							outCommands.Emplace(FSetParticleSubUVComponentPlayRateCommand{ particleSubUVComponent, playRate });
						}
						if (ImGui::Checkbox("Additive Blend", &bUseAddtiveBlend))
						{
							outCommands.Emplace(FSetParticleSubUVComponentBlendStateTypeCommand{ particleSubUVComponent, bUseAddtiveBlend ? EBlendStateType::BST_Additive : EBlendStateType::BST_AlphaBlend });
						}
					}

				}

				ImGui::EndChild();
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
	}

	ImGui::End();
}

void FEditorUIManager::updateStaticMeshProperties(UStaticMeshComponent& component, const UAssetManager& assets, FEditorCommands& outCommands)
{
    auto* mesh = component.GetStaticMesh();
    const FString meshName = mesh ? mesh->GetName().ToString() : FString();
    ImGui::TextUnformatted("Static Mesh");
    FName droppedName;
    if (AssetDragDrop::Slot("MeshSlot", "Static Mesh", meshName.CStr(), UStaticMeshAsset::GetClass(), assets, droppedName))
        outCommands.Emplace(FSetStaticMeshCommand{ &component, droppedName });
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##StaticMesh", meshName.CStr()))
    {
        for (const FName& name : UStaticMeshAsset::GetRegisteredAssetNames())
        {
            const FString label = name.ToString();
            const bool selected = mesh && mesh->GetName() == name;
            ImGui::PushID(label.CStr());
            if (ImGui::Selectable(label.CStr(), selected))
                outCommands.Emplace(FSetStaticMeshCommand{ &component, name });
            if (selected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", meshName.CStr());

    ImGui::SeparatorText("Materials");
    const int32 slotCount = (std::max)(component.GetNumMaterial(), component.GetNumOverrideMaterial());
    for (int32 slot = 0; slot < slotCount; ++slot)
    {
        ImGui::PushID(slot);
        ImGui::Text("Element %d", slot);
        if (component.GetMaterialOverride(slot))
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear Override"))
                outCommands.Emplace(FClearMaterialOverrideCommand{ &component, slot });
        }
        auto* material = component.GetMaterial(slot);
        const FString materialName = material ? material->GetName().ToString() : FString();
        if (AssetDragDrop::Slot("MaterialSlot", "Material", materialName.CStr(), UMaterial::GetClass(), assets, droppedName))
            outCommands.Emplace(FSetMaterialOverrideCommand{ &component, slot, droppedName });
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Material", materialName.CStr()))
        {
            for (const FName& name : UMaterial::GetRegisteredAssetNames())
            {
                const FString label = name.ToString();
                const bool selected = material && material->GetName() == name;
                ImGui::PushID(label.CStr());
                if (ImGui::Selectable(label.CStr(), selected))
                    outCommands.Emplace(FSetMaterialOverrideCommand{ &component, slot, name });
                if (selected) ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", materialName.CStr());
		auto& uvData = component.GetSectionUV(slot);
		if (ImGui::Checkbox("UV Scroll to X", &uvData.bUVScrollX))
		{
			outCommands.Emplace(FSetComponentUseUVScrolltoXCommand{ &component, slot, uvData.bUVScrollX });
		}
		if (ImGui::Checkbox("UV Scroll to Y", &uvData.bUVScrollY))
		{
			outCommands.Emplace(FSetComponentUseUVScrolltoYCommand{ &component,  slot, uvData.bUVScrollY });
		}
		if (ImGui::DragFloat("Scroll Speed", &uvData.UVScrollSpeed, 0.5f, 0.0f, 50.0f))
		{
			outCommands.Emplace(FSetComponentUseUVScrollSpeedCommand{ &component,  slot, uvData.UVScrollSpeed });
		}
        ImGui::PopID();
    }
}

void FEditorUIManager::updateObjectListPanelGUI(const FGuiReference& guiReference, FEditorCommands& outCommands)
{
	ImGui::SetNextWindowSize(ImVec2(340, 240), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

	AActor* selectedActor = guiReference.SceneManager.GetSelectedActor();
	ImGui::Begin("Object List Panel", nullptr, flags);
	{
		/* Object Lists */
		if (ImGui::CollapsingHeader("Object List"))
		{
			if (ImGui::BeginChild("ObjectList", ImVec2(0, 0),
				ImGuiChildFlags_Borders))
			{
                if (mGuiInputField.LastGUObjectRevision != UObject::GetGObjectRevision())
                {
                    mGuiInputField.ObjectList.Empty();
                    for (UObject* object : UObject::GetGObjectArray().ToTArray())
                        if (auto* actor = object->Cast<AActor>()) mGuiInputField.ObjectList.Add(actor);
                    mGuiInputField.LastGUObjectRevision = UObject::GetGObjectRevision();
                }
                for (const auto& reference : mGuiInputField.ObjectList)
                {
                    AActor* object = reference.Get();
                    if (!object) continue;

					const bool bSelected = object == selectedActor;
					ImGui::PushID(object); // Ensure unique ID for each child
					if (bSelected)
						ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(255, 255, 0, 50));

					if (ImGui::BeginChild("ObjectFrame", ImVec2(0, 0),
						ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY))
					{
						ImGui::Text("Class: %s", object->GetRuntimeClass()->Name.CStr());
						FString ObjectName = object->GetName().ToString();
						ImGui::Text("Name: %s | DisplayIndex: %d | ComparisonIndex: %d",
							ObjectName.CStr(),
							object->GetName().DisplayIndex,
							object->GetName().ComparisonIndex
						);

                        if (ImGui::Button("Select"))
                        {
                            mPropertyTarget = EPropertyTarget::Actor;
                            outCommands.Emplace(FSetSelectedActorCommand{ object });
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Delete"))
                            outCommands.Emplace(FDeleteActorCommand{ object });
					}

					ImGui::EndChild();
					if (bSelected)
					{
						ImGui::PopStyleColor(); // Pop the border color if it was pushed
					}

					ImGui::PopID();
				}


			}
			ImGui::EndChild();
		}

	}
	ImGui::End();
}
