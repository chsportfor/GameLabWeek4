#include "LaunchEngineLoop.h"

#include <windows.h>

#include "Core/Name.h"
#include "Core/Object/Object.h"
#include "Core/Object/ObjectFactory.h"
#include "Editor/Console.h"
#include "Editor/OverlayStat.h"
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

	RAWINPUTDEVICE rid = {};
	rid.usUsagePage = 0x01;		// Generic Desktop
	rid.usUsage = 0x02;			// Mouse
	rid.dwFlags = 0;		// 포커스 있을 때만 수신
	rid.hwndTarget = hWnd;
	RegisterRawInputDevices(&rid, 1, sizeof(rid));

	/* Init Managers */
	mRenderingPipeline = new FRenderingPipeline(hWnd);
	FrameTimer = new FFrameTimer(120);
	ViewportClient = new FEditorViewportClient(); // Todo: cChange to class
	mSceneManager = new FSceneManager(ViewportClient->GetCamera());
	mFileManager = new FFileManager();
    mAssetManager = FObjectFactory::ConstructObject<UAssetManager>();

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
	#else
		FEditorCommands editorCommands;
		mEditorUIManager->UpdateGui({
			*FrameTimer,
			*mSceneManager,
			*ViewportClient,
			*mRenderingPipeline,
			*mFileManager,
			}, editorCommands);
		processEditorCommands(editorCommands);

		
		OverlayStatWindow::GetInstance().SetStats({
			*FrameTimer,
			*mSceneManager,
			*ViewportClient,
			*mRenderingPipeline,
			*mFileManager,
			});
	#endif

		mRenderingPipeline->UpdateProjectionTransition(deltaTime);
        // Simulation precedes picking; render submission reads the final edited transforms.
        mSceneManager->Update(deltaTime);
		ViewportClient->Update(deltaTime, mRenderingPipeline->GetRenderer()->GetViewport(), mSceneManager, mRenderingPipeline->GetPerspectiveRatio());
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
			float viewportWidth = mSceneManager->GetPanelWidth();
			float viewportHeight = (1.f - ConsoleWindow::HEIGHT_RATIO) * WindowApplication.PendingHeight;

			mRenderingPipeline->OnResize(WindowApplication.PendingWidth, WindowApplication.PendingHeight);
			mRenderingPipeline->GetRenderer()->SetViewport(viewportWidth, 0, static_cast<float>(WindowApplication.PendingWidth) - viewportWidth, viewportHeight);
		#endif
			WindowApplication.bPendingResize = false;
		}

        auto Collector = mRenderingPipeline->BeginFrame(ViewportClient->GetCamera(), *mAssetManager, mSceneManager->GetSelectedActor());
        mSceneManager->SubmitRenderInfos(Collector);
		#if IS_OBJ_VIEWER
		if (mObjViewerMesh)
		{
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
				meshInfo.WorldTransformMatrix = FMatrix::Identity;
				meshInfo.Color = Material->DiffuseColor;
				meshInfo.FirstIndex = Section.FirstIndex;
				meshInfo.IndexCount = Section.IndexCount;
				Collector.MeshInfos.Add(meshInfo);
			}
		}
		#else
        ViewportClient->mGizmo.SubmitRenderInfos(Collector);
		#endif
        mRenderingPipeline->Render(Collector);

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

	delete ViewportClient;
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
	ImGui::TextDisabled("Right mouse: look  |  WASDQE: move  |  Wheel: zoom");

	if (mObjViewerError.Len() > 0)
	{
		ImGui::Separator();
		ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Load failed:");
		ImGui::TextWrapped("%s", mObjViewerError.CStr());
	}

	ImGui::End();
}

void FEngineLoop::OpenObjFileDialog()
{
	char fileName[MAX_PATH] = {};
	OPENFILENAMEA openFileName{};
	openFileName.lStructSize = sizeof(openFileName);
	openFileName.hwndOwner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
	openFileName.lpstrFilter = "OBJ Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
	openFileName.lpstrFile = fileName;
	openFileName.nMaxFile = MAX_PATH;
	openFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	openFileName.lpstrDefExt = "obj";

	if (GetOpenFileNameA(&openFileName))
	{
		LoadObjFile(fileName);
	}
}

bool FEngineLoop::LoadObjFile(std::string_view filePath)
{
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
		mObjViewerPath = filePath;
		mObjViewerError.Reset();
		mObjViewerVertexCount = mObjViewerMesh->GetVertexCount();
		mObjViewerTriangleCount = mObjViewerMesh->GetIndexCount() / 3;
		mObjViewerSectionCount = static_cast<uint32>(mObjViewerMesh->GetSections().Num());
		mObjViewerMaterialCount = static_cast<uint32>(mObjViewerMesh->GetMaterials().Num());
		FrameObjCamera(mObjViewerMesh->GetLocalBoundingBox());
		UE_LOG_F(Log, Core, "Loaded OBJ '{}': {} vertices, {} triangles.", filePath,
			mObjViewerVertexCount, mObjViewerTriangleCount);
		return true;
	}
	catch (const std::exception& exception)
	{
		mObjViewerError = std::string_view(exception.what());
		UE_LOG_F(Error, Core, "Failed to upload OBJ '{}': {}", filePath, exception.what());
		return false;
	}
}

void FEngineLoop::FrameObjCamera(const FBoundingBox& bounds)
{
	const FVector center = (bounds.Min + bounds.Max) * 0.5f;
	const float radius = FMath::Max((bounds.Max - bounds.Min).Length() * 0.5f, 0.5f);
	FCamera& camera = ViewportClient->GetCamera();
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
	ViewportClient->GetCamera().SetCameraSensitivity(command.Sensitivity);
}

void FEngineLoop::processEditorCommand(const FSetCameraFovCommand& command)
{
	ViewportClient->GetCamera().mFovDegree = command.Fov;
}

void FEngineLoop::processEditorCommand(const FSetCameraLocationCommand& command)
{
	ViewportClient->GetCamera().Location = command.Location;
}

void FEngineLoop::processEditorCommand(const FSetCameraRotationCommand& command)
{
	ViewportClient->GetCamera().Rotation = command.Rotation;
}

void FEngineLoop::processEditorCommand(const FSetGizmoModeCommand& command)
{
	ViewportClient->mGizmo.SetGizmoType(command.GizmoMode);
}

void FEngineLoop::processEditorCommand(const FCycleGizmoModeCommand& command)
{
	ViewportClient->mGizmo.CycleGizmoType();
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
		const FVector offset = selectedActor->GetTransform().Location - ViewportClient->GetCamera().Location;
		const float depth = FVector::dot(offset, ViewportClient->GetCamera().GetForwardVector());
		ViewportClient->GetCamera().mOrthoDistance = FMath::Max(depth, 0.1f);
	}
	mRenderingPipeline->StartProjectionTransition(command.bOrthographic);
}
