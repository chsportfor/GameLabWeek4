#include "EditorUIManager.h"

#include "ThirdParty/ImGui/imgui.h"
#include "ThirdParty/ImGui/imgui_impl_dx11.h"
#include "ThirdParty/ImGui/imgui_impl_win32.h"

#include "Core/FrameTimer.h"
#include "Core/IO/FileManager.h"
#include "Rendering/RenderingPipeline.h"
#include "Engine/EngineStatics.h"
#include "Engine/SceneManager.h"
#include "Engine/Components/ActorComponent.h"
#include "Engine/Components/PrimitiveComponent.h"
#include "Engine/Components/UStaticMeshComponent.h"
#include "Engine/Components/SphereComponent.h"
#include "Engine/Components/ParticleSubUVComponent.h"
#include "Core/Object/Objectiterator.h"

/* Editor */
#include "FEditorViewportClient.h"
#include "Console.h"


FEditorUIManager::FEditorUIManager(const ImGuiIO& io)
	: mGuiInputField()
	, mEditorSetting()
	, mImGuiIO(io)
{
	mPanelWidth = io.DisplaySize.x * MIN_WIDTH_RATIO;
}

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

	updateControlPanelGUI(guiReference, outCommands);
	updatePropertyWindowGUI(guiReference, outCommands);
	updateObjectListPanelGUI(guiReference, outCommands);

	ConsoleWindow::GetInstance().Draw(mPanelWidth);
}

FString saveSceneFileDialog();
FString openSceneFileDialog();
FString openObjFileDialog();

void FEditorUIManager::updateControlPanelGUI(const FGuiReference& guiReference, FEditorCommands& outCommands)
{
	float panelHeight = mImGuiIO.DisplaySize.y * CONTROL_PANEL_HEIGHT_RATIO;

	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);

	ImGui::SetNextWindowSizeConstraints(
		ImVec2(mImGuiIO.DisplaySize.x * MIN_WIDTH_RATIO, panelHeight),
		ImVec2(mImGuiIO.DisplaySize.x * MAX_WIDTH_RATIO, panelHeight)
	);
	ImGui::SetNextWindowSize(ImVec2(mPanelWidth, panelHeight), ImGuiCond_Always);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

	/* Begin ImGui Window */
	ImGui::Begin("PODO", nullptr, flags);
	mPanelWidth = ImGui::GetWindowWidth();

	ImGui::Text("FPS: %.1f  dt: %.4f", guiReference.FrameTimer.GetFPS(), guiReference.FrameTimer.GetDeltaTime());

	/* Spawn Actor */
	ImGui::SeparatorText("Spawn Actor");

	// The first three choices use the existing EPrimitive order.
	const char* actorTypeNames[] = { "Sphere", "Cube", "Triangle", "StaticMesh" };
	int32 spawnCount = mGuiInputField.SpawnCount;

	ImGui::Combo("Actor Type", &mGuiInputField.SpawnTypeIndex, actorTypeNames, IM_ARRAYSIZE(actorTypeNames));
	if (ImGui::Button("Spawn"))
	{
		if (mGuiInputField.SpawnTypeIndex == 3)
			outCommands.Emplace(FSpawnStaticMeshActorCommand{ mGuiInputField.SpawnCount });
		else
			outCommands.Emplace(FSpawnActorCommand{ static_cast<EPrimitive>(mGuiInputField.SpawnTypeIndex), mGuiInputField.SpawnCount });
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
	if (ImGui::Button("Import OBJ"))
	{
		const FString selectedFile = openObjFileDialog();
		if (selectedFile.Len() > 0)
		{
			outCommands.Emplace(FImportObjAssetCommand{selectedFile});
		}
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
	if (ImGui::Button("Test Iterator"))
	{
		for (FObjectIterator<USphereComponent> It; It; ++It)
		{
			USphereComponent* prims = *It;
			if (prims)
			{
				UE_LOG(Log, Core, "Find Primitive!");
			}
		}
	}

	//const FCamera& camera = guiReference.ViewportClient.GetCamera();

	ImGui::SeparatorText("View Mode");
	static EViewModeIndex ViewMode = EViewModeIndex::VMI_Lit;
	const char* ViewModeNames[] = { "Lit", "Unlit", "Wireframe" };

	int32 ViewModeIndex = static_cast<int32>(ViewMode);

	if (ImGui::Combo("View Mode", &ViewModeIndex, ViewModeNames, IM_ARRAYSIZE(ViewModeNames)))
	{
		ViewMode = static_cast<EViewModeIndex>(ViewModeIndex);

		outCommands.Emplace(FSetViewModeCommand{ ViewMode });
	}

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

	ImGui::Text("GridWidth");
	ImGui::SameLine();
	float gridWidth = guiReference.RenderingPipeline.GetGridWidth();
	if (ImGui::SliderFloat("##GridWidth", &gridWidth, 0.1f, 10.0f))
	{

		outCommands.Emplace(FSetGridWidthCommand{ gridWidth });
	}

	ImGui::Text("Sensitivity");
	ImGui::SameLine();
	if (ImGui::SliderFloat("##Sensitivity", &cameraSensitivity, 0.0f, 1.0f))
	{
		outCommands.Emplace(FSetCameraSensitivityCommand{ cameraSensitivity });
	}

	/* Memory Info */
	ImGui::SeparatorText("Memory Info");

	ImGui::Text("Total allocated memory count: %d", UEngineStatics::sTotalAllocationCount);
	ImGui::Text("Total allocated memory size: %d bytes", UEngineStatics::sTotalAllocationBytes);

	/* Gizmo Control */
	ImGui::SeparatorText("Gizmo Control");

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
	float controlPanelHeight = mImGuiIO.DisplaySize.y * CONTROL_PANEL_HEIGHT_RATIO;
	float propertyHeight = mImGuiIO.DisplaySize.y * WINDOW_PROPERTY_HEIGHT_RATIO;

	ImGui::SetNextWindowPos(ImVec2(0.0f, controlPanelHeight), ImGuiCond_Always);

	ImGui::SetNextWindowSizeConstraints(
		ImVec2(mImGuiIO.DisplaySize.x * MIN_WIDTH_RATIO, propertyHeight),
		ImVec2(mImGuiIO.DisplaySize.x * MAX_WIDTH_RATIO, propertyHeight)
	);
	ImGui::SetNextWindowSize(ImVec2(mPanelWidth, propertyHeight), ImGuiCond_Always);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

	ImGui::Begin("Jungle Property Window", nullptr, flags);

	mPanelWidth = ImGui::GetWindowWidth();

	AActor* selectedActor = guiReference.SceneManager.GetSelectedActor();
	if (!selectedActor)
	{
		mGuiInputField.NameEditObject.Reset();
		ImGui::TextUnformatted("Select an actor to edit its properties.");
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
						updateStaticMeshProperties(*staticMeshComponent, outCommands);
					}
					else if (UPrimitiveComponent* primitiveComponent =
						component->Cast<UPrimitiveComponent>())
					{
						bool bUseTexture = primitiveComponent->GetUseTexture();
						FLinearColor color = primitiveComponent->GetColor();

						if (ImGui::Checkbox("Use Texture", &bUseTexture))
						{
							outCommands.Emplace(FSetComponentUseTextureCommand{ primitiveComponent, bUseTexture });
						}
						if (ImGui::ColorEdit4("Color", &color.R))
						{
							outCommands.Emplace(FSetComponentColorCommand{ primitiveComponent, color });
						}
					}

					if (USphereComponent* sphereComponent =
						component->Cast<USphereComponent>())
					{
						bool bSpin = sphereComponent->GetSpin();
						float spinSpeed = sphereComponent->GetSpinSpeed();

						if (ImGui::Checkbox("Spin", &bSpin))
						{
							outCommands.Emplace(FSetSphereComponentSpinCommand{ sphereComponent, bSpin });
						}
						if (ImGui::DragFloat("Spin Speed", &spinSpeed, 0.1f, 0.0f, 3600.0f))
						{
							outCommands.Emplace(FSetSphereComponentSpinSpeedCommand{ sphereComponent, spinSpeed });
						}
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

void FEditorUIManager::updateStaticMeshProperties(UStaticMeshComponent& component, FEditorCommands& outCommands)
{
    auto* mesh = component.GetStaticMesh();
    const FString meshName = mesh ? mesh->GetName().ToString() : FString();
    ImGui::TextUnformatted("Static Mesh");
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
        ImGui::PopID();
    }
}

void FEditorUIManager::updateObjectListPanelGUI(const FGuiReference& guiReference, FEditorCommands& outCommands)
{
	float offsetHeight = mImGuiIO.DisplaySize.y * (CONTROL_PANEL_HEIGHT_RATIO + WINDOW_PROPERTY_HEIGHT_RATIO);
	float objectListPanelHeight = mImGuiIO.DisplaySize.y - offsetHeight;

	ImGui::SetNextWindowPos(ImVec2(0.0f, offsetHeight), ImGuiCond_Always);

	ImGui::SetNextWindowSizeConstraints(
		ImVec2(mImGuiIO.DisplaySize.x * MIN_WIDTH_RATIO, objectListPanelHeight),
		ImVec2(mImGuiIO.DisplaySize.x * MAX_WIDTH_RATIO, objectListPanelHeight)
	);
	ImGui::SetNextWindowSize(ImVec2(mPanelWidth, objectListPanelHeight), ImGuiCond_Always);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

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
                            outCommands.Emplace(FSetSelectedActorCommand{ object });
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
