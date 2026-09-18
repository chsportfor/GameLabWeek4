#pragma once

#include <string_view>

#include "Engine/Serialization/SceneData.h"
#include "Core/Container/TArray.h"
#include "Rendering/RenderInfo.h"
#include "Core/enum.h"
#include "Editor/EditorSetting.h"

inline constexpr std::string_view kSceneDataDir = "SceneData\\";
inline constexpr std::string_view kSceneDataSuffix = ".Scene";

class FFileManager;
class FFrameTimer;
class FEditorViewportClient;
class UWorld;
class FCamera;


class FSceneManager
{
public:
	FSceneManager(const FCamera& viewportCameraRef);
	~FSceneManager();

	void Update(float deltaTime);

	void SubmitRenderInfos(FRenderCollector& Collector) const; // TODO: 렌더인포는 UPrimitiveComponent부터 제출할 것이 있으므로 렌더링파이프라인과 연관된 자료구조에 UPrimitiveComponent 이하의 객체들이 연관되는 방식으로 교체해야 함(연관 시점, 연관 방법 등 생각해야할것들...)
	TArray<FPickInfo> GetPickInfos(const FCamera& Camera) const;

	// Clear world
	void NewScene();
	void DeleteScene();

	void SaveScene(std::string_view sceneName, const FFileManager& fileManager);
	void LoadScene(std::string_view filePath, const FFileManager& fileManager);

	UWorld* GetCurrentWorld() const { return mCurrentWorld; }

	AActor* GetSelectedActor() const { return mSelectedActor; }

	void RemoveActor(AActor* actor);

	bool IsActorSelected() const { return mSelectedActor != nullptr; }
	void SetSelectedActor(AActor* actor);
	void ResetSelectedActor() { mSelectedActor = nullptr; }

	float GetPanelWidth() const;
private:
	//static constexpr float MIN_WIDTH_RATIO = 0.2f;
	//static constexpr float MAX_WIDTH_RATIO = 0.6f;

	//static constexpr float CONTROL_PANEL_HEIGHT_RATIO = 0.4f;
	//static constexpr float WINDOW_PROPERTY_HEIGHT_RATIO = 0.3f;

	float mPanelWidth=300.0f;

	UWorld* mCurrentWorld = nullptr;
	AActor* mSelectedActor = nullptr;
	std::string LoadScenename;

	const FCamera& mViewportCameraRef;

	FEditorSetting mEditorSetting;
};
