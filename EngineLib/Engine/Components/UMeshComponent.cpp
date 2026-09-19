#include "UMeshComponent.h"

IMPLEMENT_CLASS(UMeshComponent, UPrimitiveComponent)
IMPLEMENT_SERIALIZATION(UMeshComponent, UPrimitiveComponent,{});

void UMeshComponent::SetMaterial(int32 SlotIndex, const FStaticMeshAssetMaterial* InMaterial)
{
	if (SlotIndex < 0)
	{
		return;
	}
	if (SlotIndex >= OverrideMaterials.Num())
	{
		OverrideMaterials.SetNum(SlotIndex + 1);
	}
	if (OverrideMaterials[SlotIndex] == InMaterial)
	{
		return;
	}
	OverrideMaterials[SlotIndex] = InMaterial;
}

int32 UMeshComponent::GetNumOverrideMaterial() const
{
	return OverrideMaterials.Num();
}

int32 UMeshComponent::GetNumMaterial() const
{
	return 0;
}

const FStaticMeshAssetMaterial*
UMeshComponent::GetMaterial(int32 SlotIndex) const
{
	return nullptr;
}
