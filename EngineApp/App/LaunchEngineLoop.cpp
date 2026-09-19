#include "LaunchEngineLoop.h"

#include <windows.h>

#include "Core/Name.h"
#include "Core/Object/Object.h"
#include "Core/Object/ObjectFactory.h"
#include "Editor/Console.h"
#include "Editor/EditorUIManager.h"
#include "Engine/Actor.h"
#include "Engine/Components/CubeComponent.h"
#include "Engine/Components/SphereComponent.h"
#include "Engine/Components/ParticleSubUVComponent.h"
#include "Engine/SceneManager.h"
#include "Engine/World.h"
#include "Platform/WindowApplication.h"
#include "Rendering/RenderingPipeline.h"
#include "Rendering/Renderer.h"

#include "ThirdParty/ImGui/imgui.h"
#include "ThirdParty/ImGui/imgui_impl_dx11.h"
#include "ThirdParty/ImGui/imgui_impl_win32.h"

void FEngineLoop::Init(HINSTANCE hInstance, WNDPROC WndProc)
{
	// Initialize window infos
	WCHAR WindowClass[] = L"JungleWindowClass";
	WCHAR Title[] = L"PODO";
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
	

	LayoutViewports();
	ViewportClients[0].Initialize(ELevelViewportType::Perspective);
	ViewportClients[1].Initialize(ELevelViewportType::Top);
	ViewportClients[2].Initialize(ELevelViewportType::Right);
	ViewportClients[3].Initialize(ELevelViewportType::Front);

	for(int i = 0; i < 4; i++)
		Viewports[i].SetClient(ViewportClients[i]);
	mRenderingPipeline->InitializeLoadingScreen(*mFileManager);
	mRenderingPipeline->RenderLoadingScreen();
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

	mRenderingPipeline->InitializeAssets(*mFileManager);

	mSceneManager->NewScene();

	mEditorUIManager = new FEditorUIManager(ImGui::GetIO());

	FEditorCommands startupCommands;
	mEditorUIManager->LoadSettings(startupCommands);
	processEditorCommands(startupCommands);
}

void FEngineLoop::Tick(bool bPumpMessages)
{
	if (GInTick) return;
	GInTick = true;

	FrameTimer->StartFrame();
	float deltaTime = FrameTimer->GetDeltaTime();
	ConsoleWindow& console = ConsoleWindow::GetInstance();

	//Input Threads
	{
		WindowApplication.ProcessDeferredEvents();

		//ImGui Input
		{

		}
		FEditorCommands editorCommands;
		mEditorUIManager->UpdateGui({
		*FrameTimer,
		*mSceneManager,
		GetActiveClient(),
		*mRenderingPipeline,
		*mFileManager,
			}, editorCommands);
		processEditorCommands(editorCommands);

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
			float viewportWidth = mEditorUIManager->GetPanelWidth();
			float viewportHeight = (1.f - ConsoleWindow::HEIGHT_RATIO) * WindowApplication.PendingHeight;

			mRenderingPipeline->OnResize(WindowApplication.PendingWidth, WindowApplication.PendingHeight);
			mRenderingPipeline->GetRenderer()->SetViewport(viewportWidth, 0, static_cast<float>(WindowApplication.PendingWidth) - viewportWidth, viewportHeight);
			WindowApplication.bPendingResize = false;
		}

		mRenderingPipeline->GetRenderer()->PrepareFrame();

		FMatrix projection;
		for (int i = 0; i < 4; i++) {
			// viwport 분할
			
			mRenderingPipeline->GetRenderer()->PrepareViewport(Viewports[i].GetViewport());
			
			const FCamera& cam = ViewportClients[i].GetCamera();

			FMatrix projection = ViewportClients[i].GetProjectionMatrix(Viewports[i].GetAspect());

			auto Collector = mRenderingPipeline->BeginFrame(ViewportClients[i].GetCamera(),
				Viewports[i], projection, mSceneManager->GetSelectedActor());
			mSceneManager->SubmitRenderInfos(Collector);
			if(i == ActiveViewportIndex)
				ViewportClients[i].mGizmo.SubmitRenderInfos(Collector);

			mRenderingPipeline->Render(Collector);
		}
		

        

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

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	delete mEditorUIManager;
	delete FrameTimer;
	delete mSceneManager;
	delete mFileManager;
	FObjectFactory::SetDefaultFontAsset(nullptr);
	delete mRenderingPipeline;
}

void FEngineLoop::LayoutViewports()
{
	const D3D11_VIEWPORT& full = mRenderingPipeline->GetRenderer()->GetViewport();
	const float halfWidth = full.Width * 0.5f;
	const float halfHeight = full.Height * 0.5f;

	// 왼쪽위, 오른쪽위
	Viewports[0].SetRect(full.TopLeftX, full.TopLeftY, halfWidth, halfHeight);

	Viewports[1].SetRect(full.TopLeftX + halfWidth, full.TopLeftY, halfWidth, halfHeight);

	// 왼쪽아래 오른쪽 아래
	Viewports[2].SetRect(full.TopLeftX, full.TopLeftY + halfHeight, halfWidth, halfHeight);

	Viewports[3].SetRect(full.TopLeftX + halfWidth, full.TopLeftY + halfHeight, halfWidth, halfHeight);
}

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

void FEngineLoop::processEditorCommand(const FDeleteActorCommand& command)
{
	AActor* actor = UObject::GetObjectByInternalIndex<AActor>(command.ObjectID.InternalIndex);
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
	AActor* actor = UObject::GetObjectByInternalIndex<AActor>(command.ObjectID.InternalIndex);
	if (actor)
	{
		actor->SetLocation(command.Location);
	}
}

void FEngineLoop::processEditorCommand(const FSetActorRotationCommand& command)
{
	AActor* actor = UObject::GetObjectByInternalIndex<AActor>(command.ObjectID.InternalIndex);
	if (actor)
	{
		actor->SetRotation(command.Rotation);
	}
}

void FEngineLoop::processEditorCommand(const FSetActorScaleCommand& command)
{
	AActor* actor = UObject::GetObjectByInternalIndex<AActor>(command.ObjectID.InternalIndex);
	if (actor)
	{
		actor->SetScale(command.Scale);
	}
}

void FEngineLoop::processEditorCommand(const FSetActorNameCommand& command)
{
	AActor* actor = UObject::GetObjectByInternalIndex<AActor>(command.ObjectID.InternalIndex);
	if (actor)
	{
		actor->SetName(command.NewName);
	}
}

void FEngineLoop::processEditorCommand(const FSetSelectedActorCommand& command)
{
	AActor* actor = UObject::GetObjectByInternalIndex<AActor>(command.ObjectID.InternalIndex);
	if (actor)
	{
		mSceneManager->SetSelectedActor(actor);
	}
	else
	{
		mSceneManager->ResetSelectedActor();
	}
}

void FEngineLoop::processEditorCommand(const FSetComponentUseTextureCommand& command)
{
	UPrimitiveComponent* component = UObject::GetObjectByInternalIndex<UPrimitiveComponent>(command.ObjectID.InternalIndex);
	if (component)
	{
		component->SetUseTexture(command.bUseTexture);
	}
	else
	{
		UE_LOG_F(Warning, Editor, "Component with ObjectID {} is not a UPrimitiveComponent.", command.ObjectID.InternalIndex);
	}
}

void FEngineLoop::processEditorCommand(const FSetComponentColorCommand& command)
{
	UPrimitiveComponent* component = UObject::GetObjectByInternalIndex<UPrimitiveComponent>(command.ObjectID.InternalIndex);
	if (component)
	{
		component->SetColor(command.Color);
	}
	else
	{
		UE_LOG_F(Warning, Editor, "Component with ObjectID {} is not a UPrimitiveComponent.", command.ObjectID.InternalIndex);
	}
}

void FEngineLoop::processEditorCommand(const FSetSphereComponentSpinCommand& command)
{
	USphereComponent* sphereComponent = UObject::GetObjectByInternalIndex<USphereComponent>(command.ObjectID.InternalIndex);
	if (sphereComponent)
	{
		sphereComponent->SetSpin(command.bSpin);
	}
	else
	{
		UE_LOG_F(Warning, Editor, "Component with ObjectID {} is not a USphereComponent.", command.ObjectID.InternalIndex);
	}
}

void FEngineLoop::processEditorCommand(const FSetSphereComponentSpinSpeedCommand& command)
{
	USphereComponent* sphereComponent = UObject::GetObjectByInternalIndex<USphereComponent>(command.ObjectID.InternalIndex);
	if (sphereComponent)
	{
		sphereComponent->SetSpinSpeed(command.SpinSpeed);
	}
	else
	{
		UE_LOG_F(Warning, Editor, "Component with ObjectID {} is not a USphereComponent.", command.ObjectID.InternalIndex);
	}
}

void FEngineLoop::processEditorCommand(const FSetParticleSubUVComponentLoopingCommand& command)
{
	UParticleSubUVComponent* particleComponent = UObject::GetObjectByInternalIndex<UParticleSubUVComponent>(command.ObjectID.InternalIndex);
	if (particleComponent)
	{
		particleComponent->SetLooping(command.bLooping);
	}
	else
	{
		UE_LOG_F(Warning, Editor, "Component with ObjectID {} is not a UParticleSubUVComponent.", command.ObjectID.InternalIndex);
	}
}

void FEngineLoop::processEditorCommand(const FSetParticleSubUVComponentPlayRateCommand& command)
{
	UParticleSubUVComponent* particleComponent = UObject::GetObjectByInternalIndex<UParticleSubUVComponent>(command.ObjectID.InternalIndex);
	if (particleComponent)
	{
		particleComponent->SetPlayRate(command.PlayRate);
	}
	else
	{
		UE_LOG_F(Warning, Editor, "Component with ObjectID {} is not a UParticleSubUVComponent.", command.ObjectID.InternalIndex);
	}
}

void FEngineLoop::processEditorCommand(const FSetParticleSubUVComponentBlendStateTypeCommand& command)
{
	UParticleSubUVComponent* particleComponent = UObject::GetObjectByInternalIndex<UParticleSubUVComponent>(command.ObjectID.InternalIndex);
	if (particleComponent)
	{
		particleComponent->SetBlendStateType(command.BlendStateType);
	}
	else
	{
		UE_LOG_F(Warning, Editor, "Component with ObjectID {} is not a UParticleSubUVComponent.", command.ObjectID.InternalIndex);
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
