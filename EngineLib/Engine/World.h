#pragma once

#include "Core/Object/Object.h"
#include "Actor.h"

#include "Rendering/RenderInfo.h"
//struct FRenderInfo;

class UWorld final : public UObject
{
	DECLARE_OBJECT(UWorld, UObject)
public:
	UWorld() = default;
	virtual ~UWorld();

	virtual void SerializeClass(json::JSON& outJson) const override;
	virtual void DeserializeClass(const json::JSON& inJson) override;

	void AddActor(AActor* actor);
	bool RemoveActor(uint32 componentUUID);

	const TArray<FRenderInfo>& GetRenderInfos();
	TArray<AActor*>& GetActors() { return mActors; }

	void Update(float deltaTime);
	//void Render();
	void ClearRenderInfos();

private:
	int32 getActorIndex(uint32 actorUUID) const;

private:
	enum
	{
		DEFAULT_RESERVE_MEM = 1024U
	};
	
	// Todo: Must reserve
	TArray<AActor*> mActors;

	// Todo: Maybe, move to FSceneManager
	TArray<FRenderInfo> mRenderInfos;
};
