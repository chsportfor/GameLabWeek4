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
#include "Rendering/GraphicsManager.h"
#include "Rendering/Primitives/GizmoArrow.h"
#include "Rendering/Renderer.h"
#include "Rendering/FontResource.h"

#include "ThirdParty/ImGui/imgui.h"
#include "ThirdParty/ImGui/imgui_impl_dx11.h"
#include "ThirdParty/ImGui/imgui_impl_win32.h"

// Primitive vertices definitions
#include "Rendering/Primitives/Circle.h"
#include "Rendering/Primitives/Cube.h"
#include "Rendering/Primitives/Primitives.h"
#include "Rendering/Primitives/Sphere.h"
#include "Rendering/Primitives/TexturedPrimitives.h"
#include "Rendering/Primitives/Triangle.h"

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

	RAWINPUTDEVICE rid = {};
	rid.usUsagePage = 0x01;		// Generic Desktop
	rid.usUsage = 0x02;			// Mouse
	rid.dwFlags = 0;		// 포커스 있을 때만 수신
	rid.hwndTarget = hWnd;
	RegisterRawInputDevices(&rid, 1, sizeof(rid));

	/* Init Managers */
	mGraphicsManager = new FGraphicsManager(hWnd);
	FrameTimer = new FFrameTimer(120);
	ViewportClient = new FEditorViewportClient(); // Todo: cChange to class
	mSceneManager = new FSceneManager(ViewportClient->GetCamera());
	mFileManager = new FFileManager();

	mGraphicsManager->InitializeLoadingScreen();
	mGraphicsManager->RenderLoadingScreen();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init((void*)hWnd);
	ImGui_ImplDX11_Init(mGraphicsManager->GetRenderer()->GetDevice(), mGraphicsManager->GetRenderer()->GetDeviceContext());
	ImGui::GetIO().IniFilename = "Config/imgui.ini";


	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("Assets/Fonts/malgun.ttf", 16.0f, NULL, io.Fonts->GetGlyphRangesKorean());

	/* Console Window */
	ConsoleWindow& console = ConsoleWindow::GetInstance();
	console.Init("Jungle Console Window", clientWidth);

	/* Resource Registration */
	mDefaultFontResource = new FFontResource();

	const bool jsonLoaded = mDefaultFontResource->LoadUnicodeAtlas(
		FString("Assets/Fonts/KoreanFullAtlas.json"));

	mGraphicsManager->GetRenderer()->InitializeUnicodeFont(
		L"Assets/Fonts/KoreanFullAtlas.png",
		mDefaultFontResource->GetDistanceRange());

	FObjectFactory::Initialize(*mDefaultFontResource);

	mGraphicsManager->CreateBuffer(EPrimitive::EP_Cube, Cube_vertices, static_cast<uint32>(std::size(Cube_vertices)), Cube_indices, static_cast<uint32>(std::size(Cube_indices)));
	mGraphicsManager->CreateBuffer(EPrimitive::EP_Sphere, Sphere_vertices, static_cast<uint32>(std::size(Sphere_vertices)), Sphere_indices, static_cast<uint32>(std::size(Sphere_indices)));
	mGraphicsManager->CreateBuffer(EPrimitive::EP_GizmoArrow, GizmoArrow_vertices, static_cast<uint32>(std::size(GizmoArrow_vertices)), GizmoArrow_indices, static_cast<uint32>(std::size(GizmoArrow_indices)));
	mGraphicsManager->CreateBuffer(EPrimitive::EP_Circle, Circle_vertices, static_cast<uint32>(std::size(Circle_vertices)), Circle_indices, static_cast<uint32>(std::size(Circle_indices)));
	mGraphicsManager->CreateBuffer(EPrimitive::EP_Triangle, Triangle_vertices, static_cast<uint32>(std::size(Triangle_vertices)), Triangle_indices, static_cast<uint32>(std::size(Triangle_indices)));
	mGraphicsManager->CreateBuffer(EPrimitive::EP_BillboardQuad, Quad_vertices, static_cast<uint32>(std::size(Quad_vertices)), Quad_indices, static_cast<uint32>(std::size(Quad_indices)));
	mGraphicsManager->CreateTexturedBuffer(EPrimitive::EP_Cube, CubeTextureVertices, static_cast<uint32>(std::size(CubeTextureVertices)), CubeTextureIndices, static_cast<uint32>(std::size(CubeTextureIndices)));
	mGraphicsManager->CreateTexturedBuffer(EPrimitive::EP_Sphere, SphereTextureVertices, static_cast<uint32>(std::size(SphereTextureVertices)), SphereTextureIndices, static_cast<uint32>(std::size(SphereTextureIndices)));
	mGraphicsManager->CreateTexturedBuffer(EPrimitive::EP_BillboardQuad, Quad_vertices, static_cast<uint32>(std::size(Quad_vertices)), Quad_indices, static_cast<uint32>(std::size(Quad_indices)));

	mGraphicsManager->CreatePrimitiveTexture(EPrimitive::EP_Cube, L"Assets/Textures/CubeTextureSample.dds");
	mGraphicsManager->CreatePrimitiveTexture(EPrimitive::EP_Sphere, L"Assets/Textures/EarthTexture.dds");
	mGraphicsManager->CreatePrimitiveTexture(EPrimitive::EP_BillboardQuad, L"Assets/Textures/Explosion_Alpha.dds");

	const FVector4 NearTint(1.0f, 0.65f, 0.15f, 0.85f); // 주황 = 가까운 쪽
	const FVector4 FarTint(0.25f, 0.55f, 1.0f, 0.85f); // 파랑 = 먼 쪽

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
			//mSceneManager->UpdateGUI({ *FrameTimer, mGraphicsManager, ViewportClient, mFileManager });
		}
		FEditorCommands editorCommands;
		mEditorUIManager->UpdateGui({
			*FrameTimer,
			*mSceneManager,
			*ViewportClient,
			*mGraphicsManager,
			*mFileManager,
			}, editorCommands);
		processEditorCommands(editorCommands);

		mGraphicsManager->UpdateProjectionTransition(deltaTime);
		ViewportClient->Update(deltaTime, mGraphicsManager->GetRenderer()->GetViewport(), mSceneManager, mGraphicsManager->GetPerspectiveRatio());
	}

	//Physics Threads
	{

	}

	//Game Threads
	{
		// 레이캐스트보다 먼저 돌려야 한다.
		// 여기서 RenderInfos 가 갱신되고, RayCast 가 그걸 읽는다.
		mSceneManager->Update(deltaTime);
	}

	//Render Threads
	{
		if (WindowApplication.bPendingResize)
		{
			float viewportWidth = mSceneManager->GetPanelWidth();
			float viewportHeight = (1.f - ConsoleWindow::HEIGHT_RATIO) * WindowApplication.PendingHeight;

			mGraphicsManager->GetRenderer()->OnResize(WindowApplication.PendingWidth, WindowApplication.PendingHeight);
			mGraphicsManager->GetRenderer()->SetViewport(viewportWidth, 0, static_cast<float>(WindowApplication.PendingWidth) - viewportWidth, viewportHeight);
			WindowApplication.bPendingResize = false;
		}

		mGraphicsManager->Update(deltaTime);

		mGraphicsManager->Render(
			mSceneManager->GetRenderInfos(),
			ViewportClient->mGizmo.GetGizmoRenderInfo(),
			mSceneManager->GetAxisRenderInfos(),
			ViewportClient->GetCamera(),
			mSceneManager->GetSelectedActor()
		);

		//ImGui
		{
			ImGui::Render();
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		}

		// 테스트용 쿼드 그리기
		//mGraphicsManager->GetRenderer()->RenderTestQuad();


		///
		mGraphicsManager->Display();
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

	delete ViewportClient;
	delete mEditorUIManager;
	delete FrameTimer;
	delete mSceneManager;
	delete mFileManager;
	delete mDefaultFontResource;
	delete mGraphicsManager;
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
	mGraphicsManager->SetViewMode(command.ViewMode);
}

void FEngineLoop::processEditorCommand(const FSetShowFlagCommand& command)
{
	mGraphicsManager->SetShowFlags(command.ShowFlags);
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
	mGraphicsManager->SetGridWidth(command.GridWidth);
}

void FEngineLoop::processEditorCommand(const FStartProjectionTransitionCommand& command)
{
	AActor* selectedActor = mSceneManager->GetSelectedActor();
	if (selectedActor && command.bOrthographic && mGraphicsManager->GetPerspectiveRatio() == 1.0f)
	{
		const FVector offset = selectedActor->GetTransform().Location - ViewportClient->GetCamera().Location;
		const float depth = FVector::dot(offset, ViewportClient->GetCamera().GetForwardVector());
		ViewportClient->GetCamera().mOrthoDistance = FMath::Max(depth, 0.1f);
	}
	mGraphicsManager->StartProjectionTransition(command.bOrthographic);
}
