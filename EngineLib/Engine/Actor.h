#pragma once

#include "Core/Object/Object.h"
#include "Engine/Components/ActorComponent.h"

class UWorld;
struct FRenderCollector;
struct FPickInfo;
class FCamera;
struct FTransform;
class USceneComponent;

class AActor : public UObject
{
	DECLARE_OBJECT(AActor, UObject)
public:
	AActor() = default;
	virtual ~AActor();

	void Initialize();

	virtual void SetName(const FName& name) override;

	virtual void SerializeClass(json::JSON& outJson) const override;
	virtual void DeserializeClass(const json::JSON& inJson) override;

	template<typename TComponent>
		requires std::derived_from<TComponent, UActorComponent>
	TComponent* GetComponentByType() const;

	template<typename TComponent, typename... Args>
		requires std::derived_from<TComponent, USceneComponent>
	TComponent& CreateAndAddComponent(Args&&... args);

	void AddComponent(UActorComponent* actorComponent);
	void AddRootSceneComponent(USceneComponent* sceneComponent);

	// Remove a component from the actor but does not destroy it.
	bool RemoveComponent(uint32 componentUUID);

	bool DestroyComponent(uint32 componentUUID);

	FTransform GetTransform() const;
	FRotator GetRotator() const;
	FQuat GetRotation() const;

	const TArray<UActorComponent*>& GetComponents() const { return mComponents; }

	virtual void Update(float deltaTime);

	void SubmitRenderInfos(FRenderCollector& Collector) const;
	void SubmitPickInfos(TArray<FPickInfo>& Infos, const FCamera& Camera) const;

	void SetLocation(FVector location);
	void SetRotation(FRotator rotation);
	void SetRotation(FQuat rotation);
	void SetScale(FVector scale);

private:
	int32 getComponentIndex(uint32 componentUUID) const;

private:
	
	USceneComponent* mRootComponent = nullptr;
	TArray<UActorComponent*> mComponents;
	bool mbPressed = false;
	bool mbStarted = false;
};

template<typename TComponent>
	requires std::derived_from<TComponent, UActorComponent>
TComponent* AActor::GetComponentByType() const
{
	for (UActorComponent* component : mComponents)
	{
		if (component && component->IsA<TComponent>())
		{
			return static_cast<TComponent*>(component);
		}
	}
	return nullptr;
}

template<typename TComponent, typename... Args>
	requires std::derived_from<TComponent, USceneComponent>
TComponent& AActor::CreateAndAddComponent(Args&&... args)
{
	static_assert(requires(TComponent * obj)
	{
		obj->Initialize(std::forward<Args>(args)...);
	}, "TComponent must have an Initialize method that accepts the provided arguments.");

	TComponent* component = FObjectFactory::ConstructObject<TComponent>(std::forward<Args>(args)...);

	if (component)
	{
		AddComponent(component);
		return *component;
	}
	throw std::runtime_error("Failed to create component");
}
