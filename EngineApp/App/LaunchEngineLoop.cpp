#include "LaunchEngineLoop.h"

#include <windows.h>

#include "Core/Name.h"
#include "Core/Object/Object.h"
#include "Core/Object/ObjectFactory.h"
#include "Editor/Console.h"
#include "Editor/OverlayStat.h"
#include "Editor/EditorUIManager.h"
#include "Editor/ObjViewer.h"
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

#include <cstdint>
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
	mObjViewer = new FObjViewer(*mAssetManager, *mRenderingPipeline->GetRenderer(),
		*mFileManager, ViewportClients[0]);
	mRenderingPipeline->GetRenderer()->SetViewport(0, 0, static_cast<float>(clientWidth), static_cast<float>(clientHeight));
	LayoutViewports();
	mRenderingPipeline->SetShowFlag(EEngineShowFlags::SF_Grid, false);
	mRenderingPipeline->SetShowFlag(EEngineShowFlags::SF_WorldAxis, false);
	UE_LOG(Log, Core, "HELLO OBJ VIEW");
#else
	mEditorUIManager = new FEditorUIManager(ImGui::GetIO());
	ObjViewerViewportClient.Initialize(ELevelViewportType::Perspective);
	ObjViewerViewport.SetClient(ObjViewerViewportClient);
	mObjViewer = new FObjViewer(*mAssetManager, *mRenderingPipeline->GetRenderer(),
		*mFileManager, ObjViewerViewportClient);
	mObjViewerRenderingPipeline = new FRenderingPipeline(*mRenderingPipeline->GetRenderer());
	mObjViewerRenderingPipeline->SetShowFlag(EEngineShowFlags::SF_Grid, false);
	mObjViewerRenderingPipeline->SetShowFlag(EEngineShowFlags::SF_WorldAxis, false);

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
		ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_Always);
		ImGui::Begin("OBJ Viewer", nullptr,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
		mObjViewer->DrawControls();
		ImGui::End();
		mObjViewer->UpdateControls(deltaTime, mRenderingPipeline->GetPerspectiveRatio(),
			!ImGui::GetIO().WantCaptureMouse, !ImGui::GetIO().WantCaptureKeyboard);
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
		UpdateObjViewerWindow(deltaTime);

		
		OverlayStatWindow::GetInstance().SetStats({
			*FrameTimer,
			*mSceneManager,
			GetActiveClient(),
			*mRenderingPipeline,
			*mFileManager,
			});
	#endif

		mRenderingPipeline->UpdateProjectionTransition(deltaTime);
		for (int32 i = 0; i < 4; i++)
			ViewportClients[i].SetPerspectiveRatio(mRenderingPipeline->GetPerspectiveRatio());

		// Simulation precedes picking; render submission reads the final edited transforms.

	#if IS_OBJ_VIEWER
		mRenderingPipeline->GetRenderer()->SetViewport(0, 0,
			static_cast<float>(WindowApplication.PendingWidth),
			static_cast<float>(WindowApplication.PendingHeight));
		LayoutViewports();
		mSceneManager->Update(deltaTime);
	#else
		const float panelWidth = mEditorUIManager->GetPanelWidth();
		const float renderHeight = (1.0f - ConsoleWindow::HEIGHT_RATIO) * WindowApplication.PendingHeight;
		mRenderingPipeline->GetRenderer()->SetViewport(panelWidth, 0,
			WindowApplication.PendingWidth - panelWidth, renderHeight);
		LayoutViewports();

		const FInputState& Input = WindowApplication.Input;
		ImDrawList* draw = ImGui::GetBackgroundDrawList();

		SSplitter* Splitters[] = { &RootSplitter, &LeftSplitter, &RightSplitter };
		if (!bMaximized && Input.WasPressed(VK_LBUTTON)) {
			for (SSplitter* splitter : Splitters) {
				if (splitter->GetHandleRect().Contains(Input.CursorX, Input.CursorY)) {
					DraggingSplitters.Emplace(splitter);
				}
			}
		}

		// 뭔가 눌렀으니 끌기 
		if (!DraggingSplitters.IsEmpty() && Input.IsDown(VK_LBUTTON)) {
			for (auto splitters : DraggingSplitters) {
				splitters->Drag(Input.CursorX, Input.CursorY);

				if (splitters == &LeftSplitter)
					RightSplitter.SetRatio(LeftSplitter.GetRatio());
				else if (splitters == &RightSplitter)
					LeftSplitter.SetRatio(RightSplitter.GetRatio());
			}

			

		}
		if (Input.WasReleased(VK_LBUTTON)) {
			DraggingSplitters.Empty();
		}

		// 클릭한 칸 활성화 
		if (DraggingSplitters.IsEmpty() && !bObjViewerViewportHovered
			&& (Input.WasPressed(VK_LBUTTON) || Input.WasPressed(VK_RBUTTON))) {
			for (int32 i = 0; i < 4; i++) {
				if (bMaximized && i != MaximizedIndex) continue;

				if (Viewports[i].IsHover(Input.CursorX, Input.CursorY)) {
					ActiveViewportIndex = i;
					break;
				}
			}
		}

		// 스플리터 경계선 그리기 
		for (SSplitter* splitter : Splitters) {
			if (bMaximized) break;

			const FRect handle = splitter->GetHandleRect();
			draw->AddRectFilled(
				ImVec2(handle.X, handle.Y),
				ImVec2(handle.X + handle.Width, handle.Y + handle.Height),
				IM_COL32(80, 80, 80, 255));				
		}

		// 클릭한 창 테두리 그리기
		if (!bMaximized){
			const FRect& active = Viewports[ActiveViewportIndex].GetRect();
			draw->AddRect(
				ImVec2(active.X, active.Y),
				ImVec2(active.X + active.Width, active.Y + active.Height),
				IM_COL32(255, 200, 0, 255),
				0.0f, 0, 4.0f);
		}

		mSceneManager->Update(deltaTime);
		for(int i = 0; i < 4; i++){
			if(i == ActiveViewportIndex && DraggingSplitters.IsEmpty() && !bObjViewerViewportHovered)
				// 카메라 이동, 조작
				ViewportClients[i].Update(deltaTime, Viewports[i].GetViewport(),
					mSceneManager, mRenderingPipeline->GetPerspectiveRatio());
			else {
				ViewportClients[i].GetCamera().Velocity = FVector(0.0f);
				ViewportClients[i].UpdateGizmo(mSceneManager->GetSelectedActor());
			}
		}


		// 카메라 유형 선택 창 
		const char* ViewportTypeNames[] = { "Perspective", "Top", "Right", "Front"};

		for (int viewportIndex = 0; viewportIndex < 4; viewportIndex++) {
			if (bMaximized && viewportIndex != MaximizedIndex) continue;
			const FRect& rect = Viewports[viewportIndex].GetRect();
			FEditorViewportClient& client = ViewportClients[viewportIndex];

			ImGui::SetNextWindowPos(ImVec2(rect.X + 8.0f, rect.Y + 8.0f), ImGuiCond_Always);

			char id[32];
			snprintf(id, sizeof(id), "##ViewportToolbar%d", viewportIndex);

			ImGui::Begin(id, nullptr,
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoSavedSettings);

			const ELevelViewportType current = static_cast<ELevelViewportType>(client.GetViewportType());
			ImGui::SetNextItemWidth(110.f);
			if (ImGui::BeginCombo("##Type", ViewportTypeNames[static_cast<int32>(current)])) {
				for (int32 typeIndex = 0; typeIndex < 4; typeIndex++) {
					const ELevelViewportType type = static_cast<ELevelViewportType>(typeIndex);
					if (ImGui::Selectable(ViewportTypeNames[typeIndex], type == current)) {
						client.Initialize(type);
					}
				}
				ImGui::EndCombo();
			}

			// 뷰포트 선택 버튼 
			const char* ViewportModeNames[] = { "Lit", "Unlit", "Wireframe" };
			const EViewModeIndex currentMode = client.GetViewMode();

			ImGui::SameLine();
			ImGui::SetNextItemWidth(90.0f);

			if (ImGui::BeginCombo("##ViewMode", ViewportModeNames[static_cast<int32>(currentMode)])) {
				for (int32 modeIndex = 0; modeIndex < 3; modeIndex++) {
					const EViewModeIndex mode = static_cast<EViewModeIndex>(modeIndex);
					if (ImGui::Selectable(ViewportModeNames[modeIndex], mode == currentMode)) {
						client.SetViewMode(mode);
					}
				}
				ImGui::EndCombo();
			}

			// 뷰포트 확대 버튼
			ImGui::SameLine();
			if (ImGui::Button(bMaximized ? "[+]" : "[ ]")) {
				bMaximized = !bMaximized;
				MaximizedIndex = viewportIndex;
				ActiveViewportIndex = viewportIndex;
			}

			ImGui::End();
		}


	#endif
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
			mObjViewer->SubmitRenderInfos(Collector);
			mRenderingPipeline->Render(Collector);

			#else
			for (int i = 0; i < 4; i++) {
				// viwport 분할
				if (bMaximized && i != MaximizedIndex) continue;

				mRenderingPipeline->GetRenderer()->PrepareViewport(Viewports[i].GetViewport());
			
				const FMatrix projection = ViewportClients[i].GetProjectionMatrix(Viewports[i].GetAspect());
				mRenderingPipeline->SetViewModeIndex(ViewportClients[i].GetViewMode());
				auto Collector = mRenderingPipeline->BeginFrame(ViewportClients[i].GetCamera(), *mAssetManager,
					Viewports[i], projection, mSceneManager->GetSelectedActor());
				Collector.View.PerspectiveRatio = ViewportClients[i].GetPerspectiveRatio();

				mSceneManager->SubmitRenderInfos(Collector);
				ViewportClients[i].mGizmo.SubmitRenderInfos(Collector);
				mRenderingPipeline->Render(Collector);
			}
			RenderObjViewer();
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

	if (mObjViewer)
	{
		mObjViewer->Reset();
		delete mObjViewer;
		mObjViewer = nullptr;
	}

#if !IS_OBJ_VIEWER
	ObjViewerRenderTarget.reset();
	ObjViewerDepthStencil.reset();
	delete mObjViewerRenderingPipeline;
	mObjViewerRenderingPipeline = nullptr;
#endif

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

#if !IS_OBJ_VIEWER
	mEditorUIManager->SaveSettings(RootSplitter.GetRatio(), LeftSplitter.GetRatio());
#endif

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

TArray<int32> FEngineLoop::GetPerspectiveCamera()
{
	TArray <int32> perspectiveView;
	for (int i = 0; i < 4; i++) {
		if (!ViewportClients[i].IsOrtho()) {
			perspectiveView.Add(i);
		}
	}

	return perspectiveView;
}

FViewportCameraData FEngineLoop::MakeCameraData(int32 viewportIndex)
{
	const FCamera& cam = ViewportClients[viewportIndex].GetCamera();

	FViewportCameraData data;
	data.ViewportIndex = viewportIndex;
	data.Camera.Location = cam.Location;
	data.Camera.Rotation = cam.GetRotation();
	data.Camera.FOV = cam.mFovDegree;
	data.Camera.NearClip = FCamera::NearPlane;
	data.Camera.FarClip = cam.mFarPlane;

	return data;
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
	const D3D11_VIEWPORT& viewport = mRenderingPipeline->GetRenderer()->GetViewport();
	const FRect full = { viewport.TopLeftX, viewport.TopLeftY, viewport.Width, viewport.Height };

#if IS_OBJ_VIEWER
	Viewports[0].SetRect(full);   // 뷰어: 1칸 전체
#else
	if (bMaximized)
		Viewports[MaximizedIndex].SetRect(full);
	else
		RootSplitter.SetRect(full);   // 에디터: 트리
#endif
}

#if !IS_OBJ_VIEWER
void FEngineLoop::UpdateObjViewerWindow(float DeltaTime)
{
	bObjViewerViewportHovered = false;
	if (!bObjViewerVisible)
	{
		ObjViewerViewportClient.GetCamera().Velocity = FVector(0.0f);
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(720.0f, 640.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("OBJ Viewer", &bObjViewerVisible))
	{
		mObjViewer->DrawControls();
		ImGui::Separator();

		const ImVec2 AvailableSize = ImGui::GetContentRegionAvail();
		if (AvailableSize.x >= 1.0f && AvailableSize.y >= 1.0f)
		{
			const uint32 ViewportWidth = static_cast<uint32>(AvailableSize.x);
			const uint32 ViewportHeight = static_cast<uint32>(AvailableSize.y);
			EnsureObjViewerRenderTarget(ViewportWidth, ViewportHeight);
			ObjViewerViewport.SetRect({ 0.0f, 0.0f,
				static_cast<float>(ViewportWidth), static_cast<float>(ViewportHeight) });

			const ImTextureID TextureId = static_cast<ImTextureID>(
				reinterpret_cast<uintptr_t>(ObjViewerRenderTarget->SRV.Get()));
			const ImVec2 ImageMin = ImGui::GetCursorScreenPos();
			const ImVec2 ImageSize(
				static_cast<float>(ViewportWidth), static_cast<float>(ViewportHeight));
			ImGui::InvisibleButton("##ObjViewerViewport", ImageSize,
				ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
			bObjViewerViewportHovered = ImGui::IsItemHovered();
			ImGui::GetWindowDrawList()->AddImage(ImTextureRef(TextureId), ImageMin,
				ImVec2(ImageMin.x + ImageSize.x, ImageMin.y + ImageSize.y));
		}
	}
	ImGui::End();

	mObjViewer->UpdateControls(DeltaTime, 1.0f,
		bObjViewerViewportHovered, bObjViewerViewportHovered);
}

void FEngineLoop::EnsureObjViewerRenderTarget(uint32 Width, uint32 Height)
{
	if (ObjViewerRenderTarget
		&& ObjViewerRenderTarget->Width == Width
		&& ObjViewerRenderTarget->Height == Height)
	{
		return;
	}

	URenderer* Renderer = mRenderingPipeline->GetRenderer();
	ObjViewerRenderTarget = Renderer->CreateRenderTarget2D(
		Width, Height, DXGI_FORMAT_B8G8R8A8_UNORM);
	ObjViewerDepthStencil = Renderer->CreateDepthStencil(Width, Height);
}

void FEngineLoop::RenderObjViewer()
{
	if (!bObjViewerVisible || !ObjViewerRenderTarget || !ObjViewerDepthStencil)
	{
		return;
	}

	URenderer* Renderer = mRenderingPipeline->GetRenderer();
	ID3D11ShaderResourceView* NullShaderResource = nullptr;
	Renderer->GetDeviceContext()->PSSetShaderResources(0, 1, &NullShaderResource);
	Renderer->BindRenderTarget(ObjViewerRenderTarget, ObjViewerDepthStencil);

	const FMatrix Projection = ObjViewerViewportClient.GetProjectionMatrix(
		ObjViewerViewport.GetAspect());
	auto Collector = mObjViewerRenderingPipeline->BeginFrame(
		ObjViewerViewportClient.GetCamera(), *mAssetManager,
		ObjViewerViewport, Projection);
	mObjViewer->SubmitRenderInfos(Collector);
	mObjViewerRenderingPipeline->Render(Collector);

	Renderer->BindFrameBuffer();
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
	TArray <FViewportCameraData> cameras;
	for (int32 index : GetPerspectiveCamera()) {
		cameras.Emplace(MakeCameraData(index));
	}

	mSceneManager->SaveScene(command.SceneName, *mFileManager, cameras);
}

void FEngineLoop::processEditorCommand(const FLoadSceneCommand& command)
{
	TArray <FViewportCameraData> cameras;
	mSceneManager->LoadScene(command.SceneName, *mFileManager, cameras);

	for (const FViewportCameraData& data : cameras) {
		int32 index = data.ViewportIndex;

		if (index < 0 || index >= 4) {
			TArray<int32> perspectiveView = GetPerspectiveCamera();
			if (perspectiveView.IsEmpty()) continue;

			index = perspectiveView[0];
		}


		FEditorViewportClient& client = ViewportClients[index];
		client.Initialize(ELevelViewportType::Perspective);

		FCamera& cam = client.GetCamera();
		cam.Location = data.Camera.Location;
		cam.Rotation = data.Camera.Rotation;
		cam.mFovDegree = data.Camera.FOV;
		cam.mFarPlane = data.Camera.FarClip;
	}
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

void FEngineLoop::processEditorCommand(const FToggleObjViewerCommand& command)
{
#if !IS_OBJ_VIEWER
	bObjViewerVisible = !bObjViewerVisible;
#endif
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
	GetActiveClient().SetViewMode(command.ViewMode);
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
	if (ViewportClients[ActiveViewportIndex].IsOrtho()) return;

	AActor* selectedActor = mSceneManager->GetSelectedActor();
	if (selectedActor && command.bOrthographic && mRenderingPipeline->GetPerspectiveRatio() == 1.0f)
	{
		const FVector offset = selectedActor->GetTransform().Location - ViewportClients[ActiveViewportIndex].GetCamera().Location;
		const float depth = FVector::dot(offset, ViewportClients[ActiveViewportIndex].GetCamera().GetForwardVector());
		ViewportClients[ActiveViewportIndex].GetCamera().mOrthoDistance = FMath::Max(depth, 0.1f);
	}
	mRenderingPipeline->StartProjectionTransition(command.bOrthographic);
}

void FEngineLoop::processEditorCommand(const FSetRatioVCommand& command)
{
	RootSplitter.SetRatio(command.RatioV);
}

void FEngineLoop::processEditorCommand(const FSetRatioHCommand& command)
{
	LeftSplitter.SetRatio(command.RatioH);
	RightSplitter.SetRatio(command.RatioH);
}
