
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

void UActorComponent::Update(float) {}

void UActorComponent::SubmitRenderInfos(FRenderCollector&) const {}
void UActorComponent::SubmitPickInfos(TArray<FPickInfo>&, const FCamera&) const {}
