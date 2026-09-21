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
	bool RemoveActor(AActor* actor);

	void SubmitRenderInfos(FRenderCollector& Collector) const;
	void RegisterPickTargets(FPickTargets& Targets) const;
	TArray<AActor*>& GetActors() { return mActors; }

	void Update(float deltaTime); // TODO: 액터가 액터를 삭제해도 순회가 보장되게

private:
	friend class AActor;
	void RegisterActor(AActor* actor, bool preserveName);
	void ObserveActorName(const FName& name);
	FName ResolveActorName(const FName& name, const AActor* ignoredActor, bool preserveName);
	bool IsActorNameUsed(const FName& name, const AActor* ignoredActor) const;
	int32 getActorIndex(const AActor* actor) const;
    TArray<AActor*> mActors;
	// Base-name comparison index -> next FName::Number. Keep advancing after deletion.
	// uint64 allows detecting exhaustion without wrapping the uint32 FName number.
	TMap<int32, uint64> mNextActorNameNumbers;
};
