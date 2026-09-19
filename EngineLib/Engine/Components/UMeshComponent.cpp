#include "UMeshComponent.h"

IMPLEMENT_CLASS(UMeshComponent, UPrimitiveComponent)
IMPLEMENT_SERIALIZATION(UMeshComponent, UPrimitiveComponent,{});

void UMeshComponent::SetMaterial(uint32 SlotIndex, FStaticMaterial* InMaterial)
{
	OverrideMaterials[SlotIndex] = InMaterial;
}
