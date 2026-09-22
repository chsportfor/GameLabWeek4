
#include "SceneManager.h"

#include <algorithm>
#include <format>

#include "Core/Container/TArray.h"
#include "Core/IO/FileManager.h"
#include "Core/IO/JsonUtil.h"
#include "Core/Object/ObjectFactory.h"
#include "Core/enum.h"
#include "Editor/Console.h"
#include "Editor/FEditorViewportClient.h"
#include "Engine/Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Rendering/Camera.h"

#include "ThirdParty/ImGui/imgui.h"
#include "ThirdParty/ImGui/imgui_impl_dx11.h"
#include "ThirdParty/ImGui/imgui_impl_win32.h"

#include "Core/FrameTimer.h"
#include "Engine/Components/ActorComponent.h"

FSceneManager::FSceneManager(FCamera& viewportCameraRef)
	: mViewportCameraRef(viewportCameraRef)
{
}

FSceneManager::~FSceneManager()
{
	delete mCurrentWorld;
}

void FSceneManager::Update(float deltaTime)
{
	// Todo: Save / Load
	{

	}

	mCurrentWorld->Update(deltaTime);
}

void FSceneManager::NewScene()
{
	if (mCurrentWorld != nullptr)
	{
		delete mCurrentWorld;
	}

	ResetSelectedActor();
	mCurrentWorld = FObjectFactory::ConstructObject<UWorld>();
}

void FSceneManager::DeleteScene()
{
	if (mCurrentWorld != nullptr)
	{
		delete mCurrentWorld;
		mCurrentWorld = nullptr;
	}
	ResetSelectedActor();
}

void FSceneManager::SaveScene(
	std::string_view sceneName,
	const FFileManager& fileManager, const TArray <FViewportCameraData>& cameras)
{
	FString fileName = kSceneDataDir;
	fileName += sceneName;
	fileName += kSceneDataSuffix;

	// Read the current scene data to read the Version
	uint32 version = 0;

	try
	{
		FString readSceneString = fileManager.ReadFileToString(std::filesystem::path(Utf2Wide(fileName)));
		json::JSON readSceneJson = json::JSON::Load(readSceneString);

		if (!readSceneJson.hasKey("Version") || readSceneJson.at("Version").JSONType() != json::JSON::Class::Integral)
		{
			version = 0;
		}
		else
		{
			version = readSceneJson.at("Version").ToInt();
		}
	}
	catch (const std::exception& e)
	{
		// If the file does not exist or cannot be read, we can assume it's a new scene and set version to 0
		version = 0;
	}


	json::JSON writeSceneJson = json::JSON::Make(json::JSON::Class::Object);
	json::JSON worldJson = json::JSON::Make(json::JSON::Class::Object);
	mCurrentWorld->SerializeClass(worldJson);


	writeSceneJson["Version"] = version;
	writeSceneJson["World"] = worldJson;


	json::JSON listJson = json::JSON::Make(json::JSON::Class::Array);
	for (const FViewportCameraData& camera : cameras) {
		json::JSON camJson = camera.Camera.ToJson();
		camJson["ViewportIndex"] = camera.ViewportIndex;
		listJson.append(camJson);
	}
	if (!cameras.IsEmpty()) writeSceneJson["PerspectiveCamera"] = listJson.at(0);
	writeSceneJson["PerspectiveCameras"] = listJson;

	FString jsonString = FString(writeSceneJson.dump(1, "  "));
	fileManager.WriteStringToFile(std::filesystem::path(Utf2Wide(fileName)), jsonString);

}

void FSceneManager::LoadScene(std::string_view filePath, const FFileManager& fileManager, TArray <FViewportCameraData>& outCameras)
{
	FString jsonString;

	try
	{
		jsonString = fileManager.ReadFileToString(filePath);
	}
	catch (const std::exception& e)
	{
		UE_DEBUG_LOG_ERROR_F(Core, "Failed to read scene file {}: {}", filePath, e.what());
		return;
	}

	try
	{
		json::JSON readSceneJson = json::JSON::Load(jsonString);

		if (!readSceneJson.hasKey("World") || readSceneJson.at("World").JSONType() != json::JSON::Class::Object)
		{
			throw std::runtime_error("Scene file does not contain a valid World.");
		}

		// worldJson을 수정할 수 있도록 복사본 생성
		json::JSON worldJson = readSceneJson.at("World");

		std::unique_ptr<UWorld> newWorld(FObjectFactory::LoadObject<UWorld>(worldJson));

		if (!newWorld)
		{
			throw std::runtime_error("Failed to load world.");
		}

		if (readSceneJson.hasKey("PerspectiveCameras")) {
			const json::JSON& listJson = readSceneJson.at("PerspectiveCameras");
			for (int32 i = 0; i < listJson.length(); i++) {
				outCameras.Emplace(ReadCameraEntry(listJson.at(i)));
			}
		}
		else if (readSceneJson.hasKey("PerspectiveCamera")) {
			outCameras.Emplace(ReadCameraEntry(readSceneJson.at("PerspectiveCamera")));
		}

		delete mCurrentWorld;
		mCurrentWorld = newWorld.release();

		ResetSelectedActor();
	}
	catch (const std::exception& e)
	{
		UE_DEBUG_LOG_ERROR_F(Core, "Failed to load scene file {}: {}", filePath, e.what());
	}
}

FViewportCameraData FSceneManager::ReadCameraEntry(const json::JSON& camJson)
{
	FViewportCameraData data;
	data.Camera = FCameraData(camJson);

	if (camJson.hasKey("ViewportIndex"))
		data.ViewportIndex = camJson.at("ViewportIndex").ToInt();
	else
		data.ViewportIndex = -1;

	return data;
}

void FSceneManager::RemoveActor(AActor* actor)
{
	if (mSelectedActor.Get() == actor)
	{
		ResetSelectedActor();
	}

	assert(mCurrentWorld != nullptr);
	if (!mCurrentWorld->RemoveActor(actor)) return;

	// TODO?: Consider whether to delete the actor here or manage its lifetime elsewhere.
	delete actor;
}

void  FSceneManager::SetSelectedActor(AActor* actor)
{
	if (actor == nullptr)
	{
		UE_DEBUG_LOG_WARN_F(Core, "SetSelectedActor: Attempted to set selected actor to nullptr.");
		return;
	}

    ++mSelectionRevision;
	if (actor == mSelectedActor.Get())
	{
		UE_DEBUG_LOG_F(Core, "SetSelectedActor: Actor {} is already selected.", actor->GetName().ToString());
		return; // No change
	}

	UE_DEBUG_LOG_F(Core, "SetSelectedActor: Actor {} is now selected.", actor->GetName().ToString());
	mSelectedActor = actor;
}


void FSceneManager::SubmitRenderInfos(FRenderCollector& Collector) const
{
    if (mCurrentWorld) mCurrentWorld->SubmitRenderInfos(Collector);
}

FPickTargets FSceneManager::GetPickTargets() const
{
    FPickTargets Targets;
    if (mCurrentWorld) mCurrentWorld->RegisterPickTargets(Targets);
    return Targets;
}
