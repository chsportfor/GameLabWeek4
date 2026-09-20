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
	EPrimitive PrimitiveType = EPrimitive::EP_Cube;
	int32 SpawnCount = 1;

	/* Scene Control */
	char SceneName[512] = "Default";

	/* Selected Actor Properties */
	char ActorName[384] = {};
	FObjectID NameEditObject{ -1, 0 };
	FString NameEditOriginal;

	/* Object Lists */
	TArray<UObject*> SortedObjectLists;
	uint64 LastGUObjectRevision = -1;
};

class ImGuiIO;
class FEditorUIManager
{
public:
	FEditorUIManager(const ImGuiIO& io);

	void LoadSettings(FEditorCommands& outCommands);
	void SaveSettings(const FGuiReference& guiReference);

	void UpdateGui(const FGuiReference& guiReference, FEditorCommands& outCommands);


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
	void updateObjectListPanelGUI(const FGuiReference& guiReference, FEditorCommands& outCommands);
};
