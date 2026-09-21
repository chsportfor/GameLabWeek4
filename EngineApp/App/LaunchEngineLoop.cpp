#include "LaunchEngineLoop.h"

#include <windows.h>

#include "Core/Name.h"
#include "Core/Object/Object.h"
#include "Core/Object/ObjectFactory.h"
#include "Editor/Console.h"
#include "Editor/EditorUIManager.h"
#include "Engine/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Components/UStaticMeshComponent.h"
#include "Engine/Components/CubeComponent.h"
#include "Engine/Components/SphereComponent.h"
#include "Engine/Components/ParticleSubUVComponent.h"
#include "Engine/SceneManager.h"
#include "Engine/World.h"
#include "Platform/WindowApplication.h"
#include "Rendering/RenderingPipeline.h"
#include "Rendering/Renderer.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/AssetSource/FileAssetSource.h"
#include "Rendering/BuiltinAssetNames.h"
#include "Core/AssetSystem/Asset/FontAtlasAsset.h"
#include "Engine/Assets/InitializeAssets.h"

#include <filesystem>
#include <vector>

#include "ThirdParty/ImGui/imgui.h"
#include "ThirdParty/ImGui/imgui_impl_dx11.h"
#include "ThirdParty/ImGui/imgui_impl_win32.h"

void FEngineLoop::Init(HINSTANCE hInstance, WNDPROC WndProc)
{
	// Initialize window infos
	WCHAR WindowClass[] = L"JungleWindowClass";
#if IS_OBJ_VIEWER
	WCHAR Title[] = L"PODO OBJ Viewer";
#else
	WCHAR Title[] = L"PODO";
#endif
	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };
	RegisterClassW(&wndclass);

	HWND hWnd = CreateWindowExW(
		0,
		WindowClass,
		Title,
		WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 600, 1024,
		nullptr, nullptr, hInstance, nullptr
	);

	// 창을 화면 크기에 맞게 최대화하여 표시
	ShowWindow(hWnd, SW_SHOWMAXIMIZED);
	UpdateWindow(hWnd);

	// 최대화된 후의 실제 클라이언트 크기를 구해 콘솔에 전달
	RECT clientRect;
	GetClientRect(hWnd, &clientRect);
	int clientWidth = clientRect.right - clientRect.left;
	int clientHeight = clientRect.bottom - clientRect.top;
	WindowApplication.PendingWidth = static_cast<UINT>(clientWidth);
	WindowApplication.PendingHeight = static_cast<UINT>(clientHeight);

	RAWINPUTDEVICE rid = {};
	rid.usUsagePage = 0x01;		// Generic Desktop
	rid.usUsage = 0x02;			// Mouse
	rid.dwFlags = 0;		// 포커스 있을 때만 수신
	rid.hwndTarget = hWnd;
	RegisterRawInputDevices(&rid, 1, sizeof(rid));

	/* Init Managers */
	mRenderingPipeline = new FRenderingPipeline(hWnd);
	FrameTimer = new FFrameTimer(120);
	//ViewportClient = new FEditorViewportClient(); // Todo: cChange to class
	mSceneManager = new FSceneManager(ViewportClients[0].GetCamera());
	mFileManager = new FFileManager();
	mAssetManager = FObjectFactory::ConstructObject<UAssetManager>();

	InitSplitter();
	LayoutViewports();
	ViewportClients[0].Initialize(ELevelViewportType::Perspective);
	ViewportClients[1].Initialize(ELevelViewportType::Top);
	ViewportClients[2].Initialize(ELevelViewportType::Right);
	ViewportClients[3].Initialize(ELevelViewportType::Front);

	for(int i = 0; i < 4; i++)
		Viewports[i].SetClient(ViewportClients[i]);
	
	RegisterLoadingScreenAssets(*mAssetManager, *mRenderingPipeline->GetRenderer(), *mFileManager);
	mRenderingPipeline->RenderLoadingScreen(*mAssetManager);
    mRenderingPipeline->Display();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init((void*)hWnd);
	ImGui_ImplDX11_Init(mRenderingPipeline->GetRenderer()->GetDevice(), mRenderingPipeline->GetRenderer()->GetDeviceContext());
	ImGui::GetIO().IniFilename = "Config/imgui.ini";

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("Assets/Fonts/malgun.ttf", 16.0f, NULL, io.Fonts->GetGlyphRangesKorean());

	/* Console Window */
	ConsoleWindow& console = ConsoleWindow::GetInstance();
	console.Init("Jungle Console Window", clientWidth);

	RegisterSceneAssets(*mAssetManager, *mRenderingPipeline->GetRenderer(), *mFileManager);
	    FObjectFactory::SetDefaultFontAsset(mAssetManager->GetAssetAs<UFontAtlasAsset>(BuiltinAssetNames::DefaultFont, true));
		FObjectFactory::SetDefaultAssetManager(mAssetManager);

	mSceneManager->NewScene();

#if IS_OBJ_VIEWER
	mRenderingPipeline->GetRenderer()->SetViewport(0, 0, static_cast<float>(clientWidth), static_cast<float>(clientHeight));
	mRenderingPipeline->SetShowFlag(EEngineShowFlags::SF_Grid, false);
	mRenderingPipeline->SetShowFlag(EEngineShowFlags::SF_WorldAxis, false);
	UE_LOG(Log, Core, "HELLO OBJ VIEW");
#else
	mEditorUIManager = new FEditorUIManager(ImGui::GetIO());

	FEditorCommands startupCommands;
	mEditorUIManager->LoadSettings(startupCommands);
	processEditorCommands(startupCommands);
#endif
}

void FEngineLoop::Tick(bool bPumpMessages)
{
	if (GInTick) return;
	GInTick = true;

	FrameTimer->StartFrame();
	float deltaTime = FrameTimer->GetDeltaTime();
	//Input Threads
	{
		WindowApplication.ProcessDeferredEvents();

	#if IS_OBJ_VIEWER
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		UpdateObjViewerGUI();
		UpdateObjViewerControls();
	#else
		FEditorCommands editorCommands;
		mEditorUIManager->UpdateGui({
		*FrameTimer,
		*mSceneManager,
		GetActiveClient(),
		*mRenderingPipeline,
		*mFileManager,
			}, editorCommands);
		processEditorCommands(editorCommands);
	#endif

		const float panelWidth = mEditorUIManager->GetPanelWidth();
		const float renderHeight = (1.0f - ConsoleWindow::HEIGHT_RATIO) * WindowApplication.PendingHeight;

		mRenderingPipeline->UpdateProjectionTransition(deltaTime);
		// Simulation precedes picking; render submission reads the final edited transforms.

		mRenderingPipeline->GetRenderer()->SetViewport(panelWidth, 0, WindowApplication.PendingWidth - panelWidth, renderHeight);
		LayoutViewports();

		const FInputState& Input = WindowApplication.Input;
		if (Input.WasPressed(VK_LBUTTON) || Input.WasPressed(VK_RBUTTON)) {
			for (int32 i = 0; i < 4; i++) {
				if (Viewports[i].IsHover(Input.CursorX, Input.CursorY)) {
					ActiveViewportIndex = i;
					break;
				}
			}
		}

		mSceneManager->Update(deltaTime);
		for(int i = 0; i < 4; i++){
			if(i == ActiveViewportIndex)
				// 카메라 이동, 조작
				ViewportClients[i].Update(deltaTime, Viewports[i].GetViewport(),
					mSceneManager, mRenderingPipeline->GetPerspectiveRatio());
			else {
				ViewportClients[i].GetCamera().Velocity = FVector(0.0f);
				ViewportClients[i].UpdateGizmo(mSceneManager->GetSelectedActor());
			}
		}
	}

	//Physics Threads
	{

	}

	//Render Threads
	{
		if (WindowApplication.bPendingResize)
		{
		#if IS_OBJ_VIEWER
			mRenderingPipeline->OnResize(WindowApplication.PendingWidth, WindowApplication.PendingHeight);
			mRenderingPipeline->GetRenderer()->SetViewport(0, 0,
				static_cast<float>(WindowApplication.PendingWidth), static_cast<float>(WindowApplication.PendingHeight));
		#else
			float viewportWidth = mEditorUIManager->GetPanelWidth();
			float viewportHeight = (1.f - ConsoleWindow::HEIGHT_RATIO) * WindowApplication.PendingHeight;

			mRenderingPipeline->OnResize(WindowApplication.PendingWidth, WindowApplication.PendingHeight);
			mRenderingPipeline->GetRenderer()->SetViewport(viewportWidth, 0, static_cast<float>(WindowApplication.PendingWidth) - viewportWidth, viewportHeight);
		#endif
			WindowApplication.bPendingResize = false;
		}

		mRenderingPipeline->GetRenderer()->PrepareFrame();

		#if IS_OBJ_VIEWER
			mRenderingPipeline->GetRenderer()->PrepareViewport(Viewports[0].GetViewport());
			const FMatrix projection = ViewportClients[0].GetProjectionMatrix(Viewports[0].GetAspect());
			auto Collector = mRenderingPipeline->BeginFrame(ViewportClients[0].GetCamera(), *mAssetManager,
				Viewports[0], projection, mSceneManager->GetSelectedActor());
			mSceneManager->SubmitRenderInfos(Collector);

			if (mObjViewerMesh)
		{
			const FMatrix modelTransform = FMatrix::Translation(-mObjViewerCenter)
				* FMatrix::Rotate(mObjViewerRotation)
				* FMatrix::Translation(mObjViewerCenter);
			const TArray<FMeshSection>& Sections = mObjViewerMesh->GetSections();
			const TArray<UMaterial*>& Materials = mObjViewerMesh->GetMaterials();
			for (const FMeshSection& Section : Sections)
			{
				if (Section.MaterialIndex >= static_cast<uint32>(Materials.Num())) continue;
				const UMaterial* Material = Materials[Section.MaterialIndex];
                if (!Material) continue;
				FRenderMeshInfo meshInfo{};
				meshInfo.StaticMesh = mObjViewerMesh;
				meshInfo.Texture = Material->DiffuseTexture;
				meshInfo.WorldTransformMatrix = modelTransform;
				meshInfo.Color = Material->DiffuseColor;
				meshInfo.FirstIndex = Section.FirstIndex;
				meshInfo.IndexCount = Section.IndexCount;
				Collector.MeshInfos.Add(meshInfo);
			}
		}
			mRenderingPipeline->Render(Collector);

			#else
			for (int i = 0; i < 4; i++) {
				// viwport 분할
			
				mRenderingPipeline->GetRenderer()->PrepareViewport(Viewports[i].GetViewport());
			
				const FMatrix projection = ViewportClients[i].GetProjectionMatrix(Viewports[i].GetAspect());
				auto Collector = mRenderingPipeline->BeginFrame(ViewportClients[i].GetCamera(), *mAssetManager,
					Viewports[i], projection, mSceneManager->GetSelectedActor());

				mSceneManager->SubmitRenderInfos(Collector);
				ViewportClients[i].mGizmo.SubmitRenderInfos(Collector);
				mRenderingPipeline->Render(Collector);
			}
		#endif



		//ImGui
		{
			ImGui::Render();
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		}

		mRenderingPipeline->Display();
	}

	FrameTimer->EndFrame();

	GInTick = false;
}

void FEngineLoop::End()
{
	mSceneManager->DeleteScene();

#if IS_OBJ_VIEWER
	mObjViewerMesh = nullptr;
#endif

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	delete mEditorUIManager;
	delete FrameTimer;
	delete mSceneManager;
	FObjectFactory::SetDefaultAssetManager(nullptr);
	FObjectFactory::SetDefaultFontAsset(nullptr);
    delete mAssetManager;
    mAssetManager = nullptr;
	delete mFileManager;
	delete mRenderingPipeline;
}

void FEngineLoop::InitSplitter()
{
	RootSplitter.SideLT = &LeftSplitter;
	RootSplitter.SideRB = &RightSplitter;

	LeftSplitter.SideLT = &Viewports[0];
	LeftSplitter.SideRB = &Viewports[2];
	RightSplitter.SideLT = &Viewports[1];
	RightSplitter.SideRB = &Viewports[3];
}

void FEngineLoop::LayoutViewports()
{
	const D3D11_VIEWPORT& full = mRenderingPipeline->GetRenderer()->GetViewport();
	RootSplitter.SetRect({ full.TopLeftX, full.TopLeftY, full.Width, full.Height });
}

#if IS_OBJ_VIEWER
void FEngineLoop::UpdateObjViewerGUI()
{
	ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_Always);
	ImGui::Begin("OBJ Viewer", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

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
	if (mObjViewerMesh)
	{
		ImGui::TextUnformatted("Loaded file:");
		ImGui::TextWrapped("%s", mObjViewerPath.CStr());
		ImGui::Text("Vertices: %u", mObjViewerVertexCount);
		ImGui::Text("Triangles: %u", mObjViewerTriangleCount);
		ImGui::Text("Sections: %u", mObjViewerSectionCount);
		ImGui::Text("Materials: %u", mObjViewerMaterialCount);
	}
	else
	{
		ImGui::TextDisabled("Select an OBJ file to begin.");
	}

	ImGui::Separator();
	ImGui::TextDisabled("MTL diffuse colors and textures are supported.");
	ImGui::TextDisabled("Left mouse: rotate model  |  Right mouse: look");
	ImGui::TextDisabled("WASDQE: move camera  |  Wheel: zoom");

	if (mObjViewerError.Len() > 0)
	{
		ImGui::Separator();
		ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Load failed:");
		ImGui::TextWrapped("%s", mObjViewerError.CStr());
	}

	ImGui::End();
}

void FEngineLoop::UpdateObjViewerControls()
{
	if (!mObjViewerMesh || ImGui::GetIO().WantCaptureMouse
		|| !WindowApplication.Input.IsDown(VK_LBUTTON)) return;

	constexpr float RotationSensitivity = 0.25f;
	mObjViewerRotation.Yaw = FMath::Fmod(
		mObjViewerRotation.Yaw - WindowApplication.Input.MouseDX * RotationSensitivity, 360.0f);
	mObjViewerRotation.Pitch = FMath::Fmod(
		mObjViewerRotation.Pitch - WindowApplication.Input.MouseDY * RotationSensitivity, 360.0f);
}

void FEngineLoop::OpenObjFileDialog()
{
	std::vector<wchar_t> fileName(32768, L'\0');
	OPENFILENAMEW openFileName{};
	openFileName.lStructSize = sizeof(openFileName);
	openFileName.hwndOwner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
	openFileName.lpstrFilter = L"OBJ Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
	openFileName.lpstrFile = fileName.data();
	openFileName.nMaxFile = static_cast<DWORD>(fileName.size());
	openFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	openFileName.lpstrDefExt = L"obj";

	if (GetOpenFileNameW(&openFileName))
	{
		LoadObjFile(std::filesystem::path(fileName.data()));
	}
}

bool FEngineLoop::LoadObjFile(const std::filesystem::path& filePath)
{
	const FString displayPath = Wide2Utf(filePath.wstring());
	try
	{
        const FName meshName = RegisterObjFileAsset(std::filesystem::path(filePath),
            *mAssetManager, *mRenderingPipeline->GetRenderer(), *mFileManager);
		auto loadedMesh = mAssetManager->GetAssetAs<UStaticMeshAsset>(meshName, true);
		if (!loadedMesh)
		{
			throw std::runtime_error("Failed to create the OBJ GPU mesh.");
		}
		mObjViewerMesh = std::move(loadedMesh);
		mObjViewerPath = displayPath;
		mObjViewerError.Reset();
		mObjViewerVertexCount = mObjViewerMesh->GetVertexCount();
		mObjViewerTriangleCount = mObjViewerMesh->GetIndexCount() / 3;
		mObjViewerSectionCount = static_cast<uint32>(mObjViewerMesh->GetSections().Num());
		mObjViewerMaterialCount = static_cast<uint32>(mObjViewerMesh->GetMaterials().Num());
		mObjViewerCenter = (mObjViewerMesh->GetLocalBoundingBox().Min
			+ mObjViewerMesh->GetLocalBoundingBox().Max) * 0.5f;
		mObjViewerRotation = FRotator(0.0f, 0.0f, 0.0f);
		FrameObjCamera(mObjViewerMesh->GetLocalBoundingBox());
		UE_LOG_F(Log, Core, "Loaded OBJ '{}': {} vertices, {} triangles.", displayPath,
			mObjViewerVertexCount, mObjViewerTriangleCount);
		return true;
	}
	catch (const std::exception& exception)
	{
		mObjViewerError = std::string_view(exception.what());
		UE_LOG_F(Error, Core, "Failed to upload OBJ '{}': {}", displayPath, exception.what());
		return false;
	}
}

void FEngineLoop::FrameObjCamera(const FBoundingBox& bounds)
{
	const FVector center = (bounds.Min + bounds.Max) * 0.5f;
	const float radius = FMath::Max((bounds.Max - bounds.Min).Length() * 0.5f, 0.5f);
	FCamera& camera = ViewportClients[0]->GetCamera();
	camera.Location = center + FVector(-radius * 2.5f, -radius * 2.5f, radius * 1.5f);
	camera.LookAt(center);
	camera.Velocity = FVector(0.f);
	camera.mOrthoDistance = radius * 2.5f;
	camera.mFarPlane = FMath::Max(radius * 6.0f, FCamera::FarPlane);
}
#endif

void FEngineLoop::processEditorCommands(const FEditorCommands& commands)
{
	// Process each command except for the delete command first
	for (const auto& command : commands)
	{
		if (std::holds_alternative<FDeleteActorCommand>(command))
		{
			continue; // Skip delete commands for now
		}

		std::visit(
			[this](const auto& cmd)
			{
				processEditorCommand(cmd);
			},
			command
		);
	}

	// Process delete commands last to avoid issues with dangling references
	for (const auto& command : commands)
	{
		if (const auto* deleteActorCommand =
			std::get_if<FDeleteActorCommand>(&command))
		{
			processEditorCommand(*deleteActorCommand);
		}
	}
}

void FEngineLoop::processEditorCommand(const FNewSceneCommand& command)
{
	mSceneManager->NewScene();
}

void FEngineLoop::processEditorCommand(const FSaveSceneCommand& command)
{
	mSceneManager->SaveScene(command.SceneName, *mFileManager);
}

void FEngineLoop::processEditorCommand(const FLoadSceneCommand& command)
{
	mSceneManager->LoadScene(command.SceneName, *mFileManager);
}

void FEngineLoop::processEditorCommand(const FSpawnActorCommand& command)
{
	for (int32 i = 0; i < command.SpawnCount; ++i)
	{
		AActor* newActor = FObjectFactory::SpawnPrimitiveActor(
			command.PrimitiveType,
			FVector(0, 0, 0), FRotator(0, 0, 0), FVector(1, 1, 1)
		);
		mSceneManager->GetCurrentWorld()->AddActor(newActor);
	}
}

void FEngineLoop::processEditorCommand(const FSpawnStaticMeshActorCommand& command)
{
    try
    {
        for (int32 i = 0; i < command.SpawnCount; ++i)
        {
            std::unique_ptr<AStaticMeshActor> actor(FObjectFactory::ConstructObject<AStaticMeshActor>());
            mSceneManager->GetCurrentWorld()->AddActor(actor.get());
            actor.release();
        }
    }
    catch (const std::exception& exception)
    {
        UE_LOG_F(Error, Editor, "Failed to spawn StaticMesh: {}", exception.what());
    }
}

void FEngineLoop::processEditorCommand(const FSetStaticMeshCommand& command)
{
    auto* component = command.Target.Get();
    if (!component) return;
    try
    {
        auto* mesh = mAssetManager->GetAssetAs<UStaticMeshAsset>(command.AssetName, true);
        if (!mesh) throw std::runtime_error("Static mesh asset is unavailable.");
        component->SetStaticMesh(mesh);
    }
    catch (const std::exception& exception)
    {
        UE_LOG_F(Error, Editor, "Failed to set static mesh '{}': {}", command.AssetName.ToString().CStr(), exception.what());
    }
}

void FEngineLoop::processEditorCommand(const FSetMaterialOverrideCommand& command)
{
    auto* component = command.Target.Get();
    if (!component) return;
    try
    {
        auto* material = mAssetManager->GetAssetAs<UMaterial>(command.AssetName, true);
        if (!material) throw std::runtime_error("Material asset is unavailable.");
        component->SetMaterial(command.SlotIndex, material);
    }
    catch (const std::exception& exception)
    {
        UE_LOG_F(Error, Editor, "Failed to override material '{}': {}", command.AssetName.ToString().CStr(), exception.what());
    }
}

void FEngineLoop::processEditorCommand(const FClearMaterialOverrideCommand& command)
{
    if (auto* component = command.Target.Get()) component->ClearMaterialOverride(command.SlotIndex);
}

void FEngineLoop::processEditorCommand(const FImportObjAssetCommand& command)
{
	try
	{
		const FName assetName = ImportStaticMeshObjAsset(
			std::filesystem::path(command.SourcePath.CStr()), *mAssetManager,
			*mRenderingPipeline->GetRenderer(), *mFileManager);
		UE_LOG_F(Log, Editor, "Imported OBJ '{}' as asset '{}'.",
			command.SourcePath.CStr(), assetName.ToString().CStr());
	}
	catch (const std::exception& exception)
	{
		UE_LOG_F(Error, Editor, "Failed to import OBJ '{}': {}",
			command.SourcePath.CStr(), exception.what());
	}
}

void FEngineLoop::processEditorCommand(const FDeleteActorCommand& command)
{
	AActor* actor = command.Target.Get();
	if (actor)
	{
		mSceneManager->RemoveActor(actor);
	}
}

void FEngineLoop::processEditorCommand(const FSpawnParticleCommand& command)
{
	AActor* newActor = FObjectFactory::SpawnParticleActor(
		FVector(0, 0, 0), FRotator(0, 0, 0), FVector(1, 1, 1)
	);
	mSceneManager->GetCurrentWorld()->AddActor(newActor);
}

void FEngineLoop::processEditorCommand(const FSetActorLocationCommand& command)
{
	AActor* actor = command.Target.Get();
	if (actor)
	{
		actor->SetLocation(command.Location);
	}
}

void FEngineLoop::processEditorCommand(const FSetActorRotationCommand& command)
{
	AActor* actor = command.Target.Get();
	if (actor)
	{
		actor->SetRotation(command.Rotation);
	}
}

void FEngineLoop::processEditorCommand(const FSetActorScaleCommand& command)
{
	AActor* actor = command.Target.Get();
	if (actor)
	{
		actor->SetScale(command.Scale);
	}
}

void FEngineLoop::processEditorCommand(const FSetActorNameCommand& command)
{
	AActor* actor = command.Target.Get();
	if (actor)
	{
		actor->SetName(command.NewName);
	}
}

void FEngineLoop::processEditorCommand(const FSetSelectedActorCommand& command)
{
	AActor* actor = command.Target.Get();
	if (actor)
	{
		mSceneManager->SetSelectedActor(actor);
	}
}

void FEngineLoop::processEditorCommand(const FSetComponentUseTextureCommand& command)
{
	UPrimitiveComponent* component = command.Target.Get();
	if (component)
	{
		component->SetUseTexture(command.bUseTexture);
	}
}

void FEngineLoop::processEditorCommand(const FSetComponentColorCommand& command)
{
	UPrimitiveComponent* component = command.Target.Get();
	if (component)
	{
		component->SetColor(command.Color);
	}
}

void FEngineLoop::processEditorCommand(const FSetSphereComponentSpinCommand& command)
{
	USphereComponent* sphereComponent = command.Target.Get();
	if (sphereComponent)
	{
		sphereComponent->SetSpin(command.bSpin);
	}
}

void FEngineLoop::processEditorCommand(const FSetSphereComponentSpinSpeedCommand& command)
{
	USphereComponent* sphereComponent = command.Target.Get();
	if (sphereComponent)
	{
		sphereComponent->SetSpinSpeed(command.SpinSpeed);
	}
}

void FEngineLoop::processEditorCommand(const FSetParticleSubUVComponentLoopingCommand& command)
{
	UParticleSubUVComponent* particleComponent = command.Target.Get();
	if (particleComponent)
	{
		particleComponent->SetLooping(command.bLooping);
	}
}

void FEngineLoop::processEditorCommand(const FSetParticleSubUVComponentPlayRateCommand& command)
{
	UParticleSubUVComponent* particleComponent = command.Target.Get();
	if (particleComponent)
	{
		particleComponent->SetPlayRate(command.PlayRate);
	}
}

void FEngineLoop::processEditorCommand(const FSetParticleSubUVComponentBlendStateTypeCommand& command)
{
	UParticleSubUVComponent* particleComponent = command.Target.Get();
	if (particleComponent)
	{
		particleComponent->SetBlendStateType(command.BlendStateType);
	}
}

void FEngineLoop::processEditorCommand(const FSetViewModeCommand& command)
{
	mRenderingPipeline->SetViewModeIndex(command.ViewMode);
}

void FEngineLoop::processEditorCommand(const FSetShowFlagCommand& command)
{
	mRenderingPipeline->SetShowFlags(command.ShowFlags);
}

void FEngineLoop::processEditorCommand(const FSetCameraSensitivityCommand& command)
{
	for(int i = 0; i < 4; i++)
		ViewportClients[i].GetCamera().SetCameraSensitivity(command.Sensitivity);
}

void FEngineLoop::processEditorCommand(const FSetCameraFovCommand& command)
{
	GetActiveClient().GetCamera().mFovDegree = command.Fov;
}

void FEngineLoop::processEditorCommand(const FSetCameraLocationCommand& command)
{
	GetActiveClient().GetCamera().Location = command.Location;
}

void FEngineLoop::processEditorCommand(const FSetCameraRotationCommand& command)
{
	GetActiveClient().GetCamera().Rotation = command.Rotation;
}

void FEngineLoop::processEditorCommand(const FSetGizmoModeCommand& command)
{
	for (int i = 0; i < 4; i++)
		ViewportClients[i].mGizmo.SetGizmoType(command.GizmoMode);
}

void FEngineLoop::processEditorCommand(const FCycleGizmoModeCommand& command)
{
	for (int i = 0; i < 4; i++)
		ViewportClients[i].mGizmo.CycleGizmoType();
}

void FEngineLoop::processEditorCommand(const FSetGridWidthCommand& command)
{
	mRenderingPipeline->SetGridWidth(command.GridWidth);
}

void FEngineLoop::processEditorCommand(const FStartProjectionTransitionCommand& command)
{
	AActor* selectedActor = mSceneManager->GetSelectedActor();
	if (selectedActor && command.bOrthographic && mRenderingPipeline->GetPerspectiveRatio() == 1.0f)
	{
		for (int i = 0; i < 4; i++){
			const FVector offset = selectedActor->GetTransform().Location - ViewportClients[i].GetCamera().Location;
			const float depth = FVector::dot(offset, ViewportClients[i].GetCamera().GetForwardVector());
			ViewportClients[i].GetCamera().mOrthoDistance = FMath::Max(depth, 0.1f);
		}
	}
	mRenderingPipeline->StartProjectionTransition(command.bOrthographic);
}
