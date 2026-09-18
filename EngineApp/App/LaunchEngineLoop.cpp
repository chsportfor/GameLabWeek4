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
#include "Engine/Assets/ObjImporter.h"
#include "Engine/SceneManager.h"
#include "Engine/World.h"
#include "Platform/WindowApplication.h"
#include "Rendering/RenderingPipeline.h"
#include "Rendering/Renderer.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/Asset/Texture2DAsset.h"
#include "Core/AssetSystem/AssetSource/FileAssetSource.h"
#include "Core/AssetSystem/AssetSource/StaticMeshAssetSource.h"

#include <filesystem>
#include <unordered_map>

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

        auto Collector = mRenderingPipeline->BeginFrame(ViewportClient->GetCamera(), mSceneManager->GetSelectedActor());
        mSceneManager->SubmitRenderInfos(Collector);
		#if IS_OBJ_VIEWER
		if (mObjViewerMesh)
		{
			for (const FObjViewerSection& section : mObjViewerSections)
			{
				FRenderMeshInfo meshInfo{};
				meshInfo.StaticMesh = mObjViewerMesh;
				meshInfo.Texture = section.DiffuseTexture;
				meshInfo.WorldTransformMatrix = FMatrix::Identity;
				meshInfo.Color = section.DiffuseColor;
				meshInfo.FirstIndex = section.FirstIndex;
				meshInfo.IndexCount = section.IndexCount;
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
	mObjViewerMesh.reset();
	mObjViewerSections.Reset();
#endif

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	delete ViewportClient;
	delete mEditorUIManager;
	delete FrameTimer;
	delete mSceneManager;
	delete mFileManager;
	FObjectFactory::SetDefaultFontAsset(nullptr);
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
	FStaticMesh parsedMesh;
	FString error;
	if (!FObjImporter::LoadFromFile(filePath, *mFileManager, parsedMesh, error))
	{
		mObjViewerError = error;
		UE_LOG_F(Error, Core, "Failed to load OBJ '{}': {}", filePath, error);
		return false;
	}

	try
	{
		TArray<FVertexSimple> vertices;
		vertices.Reserve(parsedMesh.Vertices.Num());
		for (const FVertexPNCT& vertex : parsedMesh.Vertices)
		{
			vertices.Add({
				vertex.Position.x, vertex.Position.y, vertex.Position.z,
				vertex.Normal.x, vertex.Normal.y, vertex.Normal.z,
				1.f, 1.f, 1.f, 1.f,
				vertex.UV.x, vertex.UV.y
			});
		}

		FStaticMeshAssetSource source(
			std::span<const FVertexSimple>(vertices.GetData(), vertices.Num()),
			std::span<const uint32>(parsedMesh.Indices.GetData(), parsedMesh.Indices.Num()));
		FStaticMeshAssetLoader loader(*mRenderingPipeline->GetRenderer());
		UAsset* asset = loader.LoadAsset(FName("ObjViewer.Current"), source);
		if (!asset)
		{
			throw std::runtime_error("Failed to create the OBJ GPU mesh.");
		}
		TSharedPtr<UStaticMeshAsset> loadedMesh(static_cast<UStaticMeshAsset*>(asset));

		TArray<FObjViewerSection> loadedSections;
		std::unordered_map<std::string, TSharedPtr<UTexture2DAsset>> textureCache;
		FTexture2DAssetLoader textureLoader(mRenderingPipeline->GetRenderer()->GetDevice());
		for (const FStaticMeshSection& section : parsedMesh.Sections)
		{
			if (section.MaterialIndex >= static_cast<uint32>(parsedMesh.Materials.Num()))
			{
				throw std::runtime_error("OBJ section references an invalid material index.");
			}

			const FStaticMaterial& material = parsedMesh.Materials[section.MaterialIndex];
			FObjViewerSection viewerSection;
			viewerSection.FirstIndex = section.FirstIndex;
			viewerSection.IndexCount = section.NumIndices;
			viewerSection.DiffuseColor = { material.DiffuseColor.x, material.DiffuseColor.y,
				material.DiffuseColor.z, material.DiffuseColor.w };

			if (material.DiffuseTexturePath.Len() > 0)
			{
				const std::string texturePath(static_cast<std::string_view>(material.DiffuseTexturePath));
				if (const auto found = textureCache.find(texturePath); found != textureCache.end())
				{
					viewerSection.DiffuseTexture = found->second;
				}
				else
				{
					FFileAssetSource textureSource(*mFileManager, std::filesystem::path(texturePath));
					UAsset* textureAsset = textureLoader.LoadAsset(FName(material.DiffuseTexturePath), textureSource);
					if (!textureAsset) throw std::runtime_error("Failed to load OBJ diffuse texture.");
					viewerSection.DiffuseTexture = TSharedPtr<UTexture2DAsset>(static_cast<UTexture2DAsset*>(textureAsset));
					textureCache.emplace(texturePath, viewerSection.DiffuseTexture);
				}
			}

			loadedSections.Add(viewerSection);
		}

		mObjViewerMesh = std::move(loadedMesh);
		mObjViewerSections = std::move(loadedSections);
		mObjViewerPath = filePath;
		mObjViewerError.Reset();
		mObjViewerVertexCount = static_cast<uint32>(parsedMesh.Vertices.Num());
		mObjViewerTriangleCount = static_cast<uint32>(parsedMesh.Indices.Num() / 3);
		mObjViewerSectionCount = static_cast<uint32>(parsedMesh.Sections.Num());
		mObjViewerMaterialCount = static_cast<uint32>(parsedMesh.Materials.Num());
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
