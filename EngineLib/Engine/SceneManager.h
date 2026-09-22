#pragma once

#include <string_view>

#include "Engine/Serialization/SceneData.h"
#include "Engine/Actor.h"
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
	FSceneManager(FCamera& viewportCameraRef);
	~FSceneManager();

	void Update(float deltaTime);

	void SubmitRenderInfos(FRenderCollector& Collector) const; // TODO: 렌더인포는 UPrimitiveComponent부터 제출할 것이 있으므로 렌더링파이프라인과 연관된 자료구조에 UPrimitiveComponent 이하의 객체들이 연관되는 방식으로 교체해야 함(연관 시점, 연관 방법 등 생각해야할것들...)
	FPickTargets GetPickTargets() const;

	// Clear world
	void NewScene();
	void DeleteScene();

	void SaveScene(std::string_view sceneName, const FFileManager& fileManager, const TArray <FViewportCameraData> &cameras);
	void LoadScene(std::string_view filePath, const FFileManager& fileManager, TArray <FViewportCameraData>& outCameras);

	static FViewportCameraData ReadCameraEntry(const json::JSON& camJson);

	UWorld* GetCurrentWorld() const { return mCurrentWorld; }

	AActor* GetSelectedActor() const { return mSelectedActor.Get(); }
    uint64 GetSelectionRevision() const { return mSelectionRevision; }

	void RemoveActor(AActor* actor);

	bool IsActorSelected() const { return mSelectedActor.IsValid(); }
	void SetSelectedActor(AActor* actor);
	void ResetSelectedActor() { mSelectedActor.Reset(); ++mSelectionRevision; }

	float GetPanelWidth() const;
private:
	//static constexpr float MIN_WIDTH_RATIO = 0.2f;
	//static constexpr float MAX_WIDTH_RATIO = 0.6f;

	//static constexpr float CONTROL_PANEL_HEIGHT_RATIO = 0.4f;
	//static constexpr float WINDOW_PROPERTY_HEIGHT_RATIO = 0.3f;

	float mPanelWidth=300.0f;

	UWorld* mCurrentWorld = nullptr;
	TWeakObjectPtr<AActor> mSelectedActor;
    uint64 mSelectionRevision = 0; // Also records reselecting the same actor.
	std::string LoadScenename;

	FCamera& mViewportCameraRef;

	FEditorSetting mEditorSetting;
};
