#pragma once

#include <Windows.h>

#include "Core/FrameTimer.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/IO/FileManager.h"
#include "Editor/FEditorViewportClient.h"
#include "Editor/FViewport.h"
#include "Editor/EditorUIManager.h"
#include "Engine/SceneManager.h"
#include "Engine/World.h"
#include "Rendering/Camera.h"
#include "Rendering/Renderer.h"
#include "Core/Math/FBoundingBox.h"
#include "Editor/SSplitterV.h"
#include "Editor/SSplitterH.h"

#include <d3d11.h>

class FRenderingPipeline;
class FObjViewer;
class FEngineLoop
{
public:
	FEngineLoop()
	{
	}
	~FEngineLoop() {};

	void Init(HINSTANCE hInstance, WNDPROC WndProc);
	void Tick(bool bPumpMessages);
	void End();

	FEditorViewportClient& GetActiveClient() { return ViewportClients[ActiveViewportIndex]; }
	TArray<int32> GetPerspectiveCamera();
	FViewportCameraData MakeCameraData(int32 viewportIndex);

	void InitSplitter();
	void LayoutViewports();

private:
#if !IS_OBJ_VIEWER
	void UpdateObjViewerWindow(float DeltaTime);
	void EnsureObjViewerRenderTarget(uint32 Width, uint32 Height);
	void RenderObjViewer();
#endif

	// Todo: Make as pointer
	FFrameTimer* FrameTimer = nullptr;
	bool GInTick = false;

	FEditorViewportClient ViewportClients[4];
	FViewport Viewports[4];
	int32 ActiveViewportIndex = 0;

	FRenderingPipeline* mRenderingPipeline = nullptr;
	FSceneManager* mSceneManager = nullptr;
	FEditorUIManager* mEditorUIManager = nullptr;
	UAssetManager* mAssetManager = nullptr;
	FObjViewer* mObjViewer = nullptr;

	SSplitterV RootSplitter;
	SSplitterH LeftSplitter;
	SSplitterH RightSplitter;
	TArray <SSplitter*> DraggingSplitters;

	bool bMaximized = false;
	int32 MaximizedIndex = 0;



#if !IS_OBJ_VIEWER
	FRenderingPipeline* mObjViewerRenderingPipeline = nullptr;
	FEditorViewportClient ObjViewerViewportClient;
	FViewport ObjViewerViewport;
	TSharedPtr<FRenderTarget2D> ObjViewerRenderTarget;
	TSharedPtr<FDepthStencil> ObjViewerDepthStencil;
	bool bObjViewerVisible = false;
	bool bObjViewerViewportHovered = false;
#endif


	/* Editor Command */
	void processEditorCommands(const FEditorCommands& commands);
	void processEditorCommand(const FNewSceneCommand& command);
	void processEditorCommand(const FSaveSceneCommand& command);
	void processEditorCommand(const FLoadSceneCommand& command);

	void processEditorCommand(const FSpawnStaticMeshActorCommand& command);
	void processEditorCommand(const FSetStaticMeshCommand& command);
	void processEditorCommand(const FSetMaterialOverrideCommand& command);
	void processEditorCommand(const FClearMaterialOverrideCommand& command);
	void processEditorCommand(const FImportObjAssetCommand& command);
	void processEditorCommand(const FOpenStaticMeshImportCommand& command);
	void processEditorCommand(const FDeleteActorCommand& command);
	void processEditorCommand(const FSpawnParticleCommand& command);

	void processEditorCommand(const FSetActorLocationCommand& command);
	void processEditorCommand(const FSetActorRotationCommand& command);
	void processEditorCommand(const FSetActorScaleCommand& command);
	void processEditorCommand(const FSetActorNameCommand& command);
	void processEditorCommand(const FSetSelectedActorCommand& command);

	void processEditorCommand(const FSetComponentColorCommand& command);
	void processEditorCommand(const FSetParticleSubUVComponentLoopingCommand& command);
	void processEditorCommand(const FSetParticleSubUVComponentPlayRateCommand& command);
	void processEditorCommand(const FSetParticleSubUVComponentBlendStateTypeCommand& command);

	void processEditorCommand(const FSetViewModeCommand& command);
	void processEditorCommand(const FSetShowFlagCommand& command);
	void processEditorCommand(const FSetCameraSensitivityCommand& command);
	void processEditorCommand(const FSetCameraFovCommand& command);
	void processEditorCommand(const FSetCameraLocationCommand& command);
	void processEditorCommand(const FSetCameraRotationCommand& command);
	void processEditorCommand(const FSetGizmoModeCommand& command);
	void processEditorCommand(const FCycleGizmoModeCommand& command);
	void processEditorCommand(const FSetGizmoWorldModeCommand& command);

	void processEditorCommand(const FSetGridWidthCommand& command);
	void processEditorCommand(const FStartProjectionTransitionCommand& command);

	void processEditorCommand(const FSetComponentUseUVScrolltoXCommand& command);
	void processEditorCommand(const FSetComponentUseUVScrolltoYCommand& command);
	void processEditorCommand(const FSetComponentUseUVScrollSpeedCommand& command);

	void processEditorCommand(const FSetRatioVCommand& command);
	void processEditorCommand(const FSetRatioHCommand& command);

};

inline FEngineLoop GEngineLoop;
