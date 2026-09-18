#pragma once

#include "Core/Object/Object.h"
#include "Actor.h"

#include "Rendering/RenderInfo.h"

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

	void SubmitRenderInfos(FRenderCollector& Collector) const;
	void SubmitPickInfos(TArray<FPickInfo>& Infos, const FCamera& Camera) const;
	TArray<AActor*>& GetActors() { return mActors; }

	void Update(float deltaTime);

private:
	int32 getActorIndex(uint32 actorUUID) const;
    TArray<AActor*> mActors;
};
