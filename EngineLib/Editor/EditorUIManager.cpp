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
	//outCommands.Emplace(FSetCameraLocationCommand{ mEditorSetting.CameraLocation});
	//outCommands.Emplace(FSetCameraRotationCommand{ mEditorSetting.CameraRotation });
	//outCommands.Emplace(FSetCameraFovCommand{ mEditorSetting.CameraFOV });
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
	// NOTE: This name array must be edited when adding new primitive types to EPrimitive enum.
	ImGui::SeparatorText("Spawn Actor");

	const char* primitiveTypeNames[] = { "Sphere", "Cube", "Triangle" };
	int32 primitiveTypeIndex = static_cast<int32>(mGuiInputField.PrimitiveType);
	int32 spawnCount = mGuiInputField.SpawnCount;

	if (ImGui::Combo("Primitive Type", &primitiveTypeIndex, primitiveTypeNames, IM_ARRAYSIZE(primitiveTypeNames)))
	{
		mGuiInputField.PrimitiveType = static_cast<EPrimitive>(primitiveTypeIndex);
	}
	if (ImGui::Button("Spawn"))
	{
		outCommands.Emplace(FSpawnActorCommand{ mGuiInputField.PrimitiveType, mGuiInputField.SpawnCount });
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

	/* Actor Transform */
	ImGui::SeparatorText("Actor Transform");
	const AActor* selectedActor = guiReference.SceneManager.GetSelectedActor();
	if (selectedActor)
	{
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
			outCommands.Emplace(FSetActorLocationCommand{ selectedActor->GetObjectID(), translationInput });
		}
		if (ImGui::DragFloat3("Rotation", &rotationInput[0], 0.1f))
		{
			//mSelectedActor->SetRotation(FRotator{
			//	rotationInput[1], // Pitch
			//	rotationInput[2], // Yaw
			//	rotationInput[0]  // Roll
			//	});

			outCommands.Emplace(FSetActorRotationCommand{ selectedActor->GetObjectID(), FRotator{
				rotationInput[1], // Pitch
				rotationInput[2], // Yaw
				rotationInput[0]  // Roll
				} });
		}
		if (ImGui::DragFloat3("Scale", &scaleInput.x, 0.1f, MIN_SCALE, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp))
		{
			//mSelectedActor->SetScale(scaleInput);
			outCommands.Emplace(FSetActorScaleCommand{ selectedActor->GetObjectID(), scaleInput });
		}

		/* Components */
		ImGui::SeparatorText("Components");
		if (ImGui::BeginChild("Components", ImVec2(0, 0), ImGuiChildFlags_Borders))
		{
			const TArray<UActorComponent*>& components = selectedActor->GetComponents();
			for (const UActorComponent* component : components)
			{
				ImGui::PushID(component->UUID); // Ensure unique ID for each child
				if (ImGui::BeginChild("ComponentFrame", ImVec2(0, 0),
					ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY))
				{
					ImGui::Text("Class: %s", component->GetRuntimeClass()->Name.CStr());
					ImGui::Text("UUID: %d", component->UUID);
					FString ComponentName = component->GetName().ToString();
					ImGui::Text("Name: %s | DisplayIndex: %d | ComparisonIndex: %d",
						ComponentName.CStr(),
						component->GetName().DisplayIndex,
						component->GetName().ComparisonIndex
					);
				}

				if (const UPrimitiveComponent* primitiveComponent =
					component->Cast<UPrimitiveComponent>())
				{
					bool bUseTexture = primitiveComponent->GetUseTexture();
					FLinearColor color = primitiveComponent->GetColor();

					if (ImGui::Checkbox("Use Texture", &bUseTexture))
					{
						outCommands.Emplace(FSetComponentUseTextureCommand{ primitiveComponent->GetObjectID(), bUseTexture });
					}
					if (ImGui::ColorEdit4("Color", &color.R))
					{
						outCommands.Emplace(FSetComponentColorCommand{ primitiveComponent->GetObjectID(), color });
					}
				}

				if (const USphereComponent* sphereComponent =
					component->Cast<USphereComponent>())
				{
					bool bSpin = sphereComponent->GetSpin();
					float spinSpeed = sphereComponent->GetSpinSpeed();

					if (ImGui::Checkbox("Spin", &bSpin))
					{
						outCommands.Emplace(FSetSphereComponentSpinCommand{ sphereComponent->GetObjectID(), bSpin });
					}
					if (ImGui::DragFloat("Spin Speed", &spinSpeed, 0.1f, 0.0f, 3600.0f))
					{
						outCommands.Emplace(FSetSphereComponentSpinSpeedCommand{ sphereComponent->GetObjectID(), spinSpeed });
					}
				}

				if (const UParticleSubUVComponent* particleSubUVComponent =
					component->Cast<UParticleSubUVComponent>())
				{
					bool bLooping = particleSubUVComponent->IsLooping();
					float playRate = particleSubUVComponent->GetPlayRate();
					bool bUseAddtiveBlend = particleSubUVComponent->GetBlendStateType() == EBlendStateType::BST_Additive;

					if (ImGui::Checkbox("Looping", &bLooping))
					{
						outCommands.Emplace(FSetParticleSubUVComponentLoopingCommand{ particleSubUVComponent->GetObjectID(), bLooping });
					}
					if (ImGui::DragFloat("Play Rate", &playRate, 0.1f, 0.0f, 10.0f))
					{
						outCommands.Emplace(FSetParticleSubUVComponentPlayRateCommand{ particleSubUVComponent->GetObjectID(), playRate });
					}
					if (ImGui::Checkbox("Additive Blend", &bUseAddtiveBlend))
					{
						outCommands.Emplace(FSetParticleSubUVComponentBlendStateTypeCommand{ particleSubUVComponent->GetObjectID(), bUseAddtiveBlend ? EBlendStateType::BST_Additive : EBlendStateType::BST_AlphaBlend });
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

	const AActor* selectedActor = guiReference.SceneManager.GetSelectedActor();
	ImGui::Begin("Object List Panel", nullptr, flags);
	{
		/* Object Lists */
		if (ImGui::CollapsingHeader("Object List"))
		{
			if (ImGui::BeginChild("ObjectList", ImVec2(0, 0),
				ImGuiChildFlags_Borders))
			{
				// Update and sort the object list only if there has been a change in the global object revision
				if (mGuiInputField.LastGUObjectRevision != UObject::GetGObjectRevision())
				{
					mGuiInputField.SortedObjectLists = UObject::GetGObjectArray().ToTArray();
					mGuiInputField.LastGUObjectRevision = UObject::GetGObjectRevision();

					// Sort the objects by UUID
					std::sort(mGuiInputField.SortedObjectLists.begin(), mGuiInputField.SortedObjectLists.end(),
						[](UObject* a, UObject* b) { return a->UUID < b->UUID; });
				}

				int32 selectedActorUUID = selectedActor
					? selectedActor->UUID
					: -1;

				//UObject* bDeleteActorOrNull = nullptr;

				static char NameBuffer[384] = {};
				static int32 CachedSelectedUUID = -1;

				//for (unsigned int objectsIndex = 0; objectsIndex < mGuiInputField.SortedObjectLists.Num(); ++objectsIndex)
				//{
				//	UObject* object = mGuiInputField.SortedObjectLists[objectsIndex];
				for (const UObject* object : mGuiInputField.SortedObjectLists)
				{
					if (!object->IsA<AActor>())
					{
						continue;
					}

					bool bSelected = false;
					ImGui::PushID(object->UUID); // Ensure unique ID for each child

					if (ImGui::BeginChild("ObjectFrame", ImVec2(0, 0),
						ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY))
					{
						ImGui::Text("Class: %s", object->GetRuntimeClass()->Name.CStr());
						ImGui::Text("UUID: %d", object->UUID);
						FString ObjectName = object->GetName().ToString();
						ImGui::Text("Name: %s | DisplayIndex: %d | ComparisonIndex: %d",
							ObjectName.CStr(),
							object->GetName().DisplayIndex,
							object->GetName().ComparisonIndex
						);

						// TODO: Move implement delete to where?
						if (object->IsA<AActor>())
						{
							const AActor* actor = object->Cast<AActor>();

							if (ImGui::Button("Select"))
							{
								//SetSelectedActor(actor);
								outCommands.Emplace(FSetSelectedActorCommand{ actor->GetObjectID() });
							}
							else
							{
								ImGui::SameLine();
								if (ImGui::Button("Delete"))
								{
									//bDeleteActorOrNull = object;
									outCommands.Emplace(FDeleteActorCommand{ actor->GetObjectID() });
								}
							}
						}
					}

					// Highlight the frame if this object is the clicked actor
					if (object->UUID == selectedActorUUID)
					{
						bSelected = true;
						ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(255, 255, 0, 50)); // Light yellow background

						if (CachedSelectedUUID != object->UUID)
						{
							CachedSelectedUUID = object->UUID;

							FString CurrentName = object->GetName().ToString();

							strcpy_s(NameBuffer, sizeof(NameBuffer), CurrentName.CStr());
						}

						ImGui::Text("Edit Name");
						ImGui::SameLine();
						bool bEnterPressed = ImGui::InputText("##Edit Name", NameBuffer, sizeof(NameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
						ImGui::SameLine();
						bool bApplyPressed = ImGui::Button("Apply");

						if (bEnterPressed || bApplyPressed)
						{
							//object->SetName(FName(NameBuffer));
							outCommands.Emplace(FSetActorNameCommand{ object->GetObjectID(), FName(NameBuffer) });
						}
					}

					if (bSelected)
					{
						ImGui::PopStyleColor(); // Pop the border color if it was pushed
					}

					ImGui::EndChild();

					ImGui::PopID();
				}

				//if (bDeleteActorOrNull != nullptr)
				//{
				//	AActor* deleteActor = bDeleteActorOrNull->Cast<AActor>();

				//	if (selectedActor != nullptr && selectedActor->UUID == deleteActor->UUID)
				//	{
				//		selectedActor = nullptr;
				//	}

				//	assert(mCurrentWorld != nullptr);
				//	mCurrentWorld->RemoveActor(deleteActor->UUID);

				//	delete deleteActor;
				//}

			}
			ImGui::EndChild();
		}

	}
	ImGui::End();
}
