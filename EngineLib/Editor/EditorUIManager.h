#pragma once

#include <variant>

#include "Core/Core.h"
#include "Core/Object/Object.h"
#include "Core/enum.h"

#include "EditorSetting.h"
#include "EditorCommands.h"

/* Foward Declarations */
class ImGuiIO;
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

class ImGuiIO;
class FEditorUIManager
{
public:
	FEditorUIManager(const ImGuiIO& io);

	void LoadSettings(FEditorCommands& outCommands);
	void SaveSettings(float ratioV, float ratioH);

	void UpdateGui(const FGuiReference& guiReference, FEditorCommands& outCommands);

	float GetPanelWidth() { return mPanelWidth; }
	void SetPanelWidth(float inWidth) { mPanelWidth = inWidth; }

private:
	// Internal state for ImGui input fields and other GUI elements
	FGuiInputField mGuiInputField;
	FEditorSetting mEditorSetting;

	const ImGuiIO& mImGuiIO;
	/*ID3D11ShaderResourceView* mLoadingScreenSRV = nullptr;*/

	float mPanelWidth = 300.0f; // Default width for the property and object list panels

	static constexpr float MIN_WIDTH_RATIO = 0.2f;
	static constexpr float MAX_WIDTH_RATIO = 0.6f;

	static constexpr float CONTROL_PANEL_HEIGHT_RATIO = 0.45f;
	static constexpr float WINDOW_PROPERTY_HEIGHT_RATIO = 0.3f;

	void updateControlPanelGUI(const FGuiReference& guiReference, FEditorCommands& outCommands);
	void updatePropertyWindowGUI(const FGuiReference& guiReference, FEditorCommands& outCommands);
	void updateStaticMeshProperties(UStaticMeshComponent& component, FEditorCommands& outCommands);
	void updateObjectListPanelGUI(const FGuiReference& guiReference, FEditorCommands& outCommands);
};
