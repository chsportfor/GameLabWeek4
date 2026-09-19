#pragma once

#include <Windows.h>

#include "Core/FrameTimer.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/IO/FileManager.h"
#include "Editor/FEditorViewportClient.h"
#include "Editor/EditorUIManager.h"
#include "Engine/SceneManager.h"
#include "Engine/World.h"
#include "Rendering/Camera.h"
#include "Rendering/Renderer.h"
#include "Core/Math/FBoundingBox.h"

#include <d3d11.h>

class Sphere;
class FRenderingPipeline;
class FFileAssetSource;
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
private:
	// Todo: Make as pointer
	FFrameTimer* FrameTimer = nullptr;
	bool GInTick = false;
	FEditorViewportClient* ViewportClient = nullptr;

	FRenderingPipeline* mRenderingPipeline = nullptr;
	FSceneManager* mSceneManager = nullptr;
	FFileManager* mFileManager = nullptr;
	FEditorUIManager* mEditorUIManager = nullptr;
	FAssetManager mAssetManager;


#if IS_OBJ_VIEWER
	void UpdateObjViewerGUI();
	void OpenObjFileDialog();
	bool LoadObjFile(std::string_view filePath);
	void FrameObjCamera(const FBoundingBox& bounds);

	TSharedPtr<FStaticMeshAsset> mObjViewerMesh;
	TSharedPtr<FFileAssetSource> mObjViewerMeshSource;
	FString mObjViewerPath;
	FString mObjViewerError;
	uint32 mObjViewerVertexCount = 0;
	uint32 mObjViewerTriangleCount = 0;
	uint32 mObjViewerSectionCount = 0;
	uint32 mObjViewerMaterialCount = 0;
#endif


	/* Editor Command */
	void processEditorCommands(const FEditorCommands& commands);
	void processEditorCommand(const FNewSceneCommand& command);
	void processEditorCommand(const FSaveSceneCommand& command);
	void processEditorCommand(const FLoadSceneCommand& command);

	void processEditorCommand(const FSpawnActorCommand& command);
	void processEditorCommand(const FDeleteActorCommand& command);
	void processEditorCommand(const FSpawnParticleCommand& command);

	void processEditorCommand(const FSetActorLocationCommand& command);
	void processEditorCommand(const FSetActorRotationCommand& command);
	void processEditorCommand(const FSetActorScaleCommand& command);
	void processEditorCommand(const FSetActorNameCommand& command);
	void processEditorCommand(const FSetSelectedActorCommand& command);

	void processEditorCommand(const FSetComponentUseTextureCommand& command);
	void processEditorCommand(const FSetComponentColorCommand& command);
	void processEditorCommand(const FSetSphereComponentSpinCommand& command);
	void processEditorCommand(const FSetSphereComponentSpinSpeedCommand& command);
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

	void processEditorCommand(const FSetGridWidthCommand& command);
	void processEditorCommand(const FStartProjectionTransitionCommand& command);
};

inline FEngineLoop GEngineLoop;
