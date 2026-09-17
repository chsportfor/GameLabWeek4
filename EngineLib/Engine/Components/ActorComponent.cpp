
#include "ActorComponent.h"

IMPLEMENT_CLASS(UActorComponent, UObject);

UActorComponent::UActorComponent()
	: mOwner(nullptr)
{
}

UActorComponent::~UActorComponent()
{
}

void UActorComponent::SetOwner(AActor* owner)
{
	assert(mOwner == nullptr);

	mOwner = owner;
}

void UActorComponent::ClearOwner()
{
	mOwner = nullptr;
}

AActor* UActorComponent::GetOwner() const
{
	return mOwner;
}

void UActorComponent::Update(float deltaTime, TArray<FRenderInfo>* outRenderInfos)
{
	// Todo: Do nothing, must override, some components may not call Update()
	// assert(false);
}

void UActorComponent::GetRenderInfos(TArray<FRenderInfo>* outRenderInfos) const
{
	// Todo: Do nothing, must override, some components may not call GetRenderInfos()
	// assert(false);
}
