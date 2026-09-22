#pragma once

#include <variant>

#include "Core/Core.h"
#include "Core/Object/Object.h"
#include "Core/enum.h"

#include "EditorSetting.h"
#include "EditorCommands.h"
#include "SWindow.h"

/* Foward Declarations */
class URenderer;
class FFrameTimer;
class FRenderingPipeline;
class FEditorViewportClient;
class FFileManager;
class FSceneManager;
class UStaticMeshComponent;

struct ID3D11ShaderResourceView;

struct FGuiReference
{
	const FFrameTimer& FrameTimer;
	const FSceneManager& SceneManager;
	const FEditorViewportClient& ViewportClient;
	const FRenderingPipeline& RenderingPipeline;
	const FFileManager& FileManager;
};

struct FGuiInputField
{
	/* Spawn Actor */
	int32 SpawnTypeIndex = 1;
	int32 SpawnCount = 1;

	/* Scene Control */
	char SceneName[512] = "Default";

	/* Selected Actor Properties */
	char ActorName[384] = {};
	TWeakObjectPtr<AActor> NameEditObject;
	FString NameEditOriginal;

	/* Object Lists */
	TArray<TWeakObjectPtr<AActor>> ObjectList;
	uint64 LastGUObjectRevision = -1;
};

class FEditorUIManager
{
public:
	FEditorUIManager() = default;

	void LoadSettings(FEditorCommands& outCommands);
	void SaveSettings(float ratioV, float ratioH);

	void UpdateGui(const FGuiReference& guiReference, FEditorCommands& outCommands);

	const FRect& GetSceneViewportRect() const { return mSceneViewportRect; }

private:
	// Internal state for ImGui input fields and other GUI elements
	FGuiInputField mGuiInputField;
	FEditorSetting mEditorSetting;

	FRect mSceneViewportRect{};
	void updateDockSpace();

	void updateControlPanelGUI(const FGuiReference& guiReference, FEditorCommands& outCommands);
	void updatePropertyWindowGUI(const FGuiReference& guiReference, FEditorCommands& outCommands);
	void updateStaticMeshProperties(UStaticMeshComponent& component, FEditorCommands& outCommands);
	void updateObjectListPanelGUI(const FGuiReference& guiReference, FEditorCommands& outCommands);
};
