#include "UMeshComponent.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(UMeshComponent, UPrimitiveComponent)
IMPLEMENT_SERIALIZATION(UMeshComponent, UPrimitiveComponent,{});

void UMeshComponent::SetMaterial(int32 SlotIndex, UMaterial* InMaterial)
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

UMaterial* UMeshComponent::GetMaterialOverride(int32 SlotIndex) const
{
    return SlotIndex >= 0 && SlotIndex < OverrideMaterials.Num() ? OverrideMaterials[SlotIndex] : nullptr;
}

void UMeshComponent::ClearMaterialOverride(int32 SlotIndex)
{
    if (SlotIndex >= 0 && SlotIndex < OverrideMaterials.Num())
        OverrideMaterials[SlotIndex] = nullptr;
}

int32 UMeshComponent::GetNumMaterial() const
{
	return 0;
}

UMaterial*
UMeshComponent::GetMaterial(int32 SlotIndex) const
{
	return nullptr;
}

std::span<const FPropertyInfo> UMeshComponent::GetDeclaredProperties()
{
    static const FPropertyInfo Properties[] = {REFLECT_PROPERTY(UMeshComponent, OverrideMaterials)};
    return Properties;
}
