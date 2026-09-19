#pragma once

#include "PrimitiveComponent.h"
#include "Core/Object/ObjectFactory.h"
#include "Core/Container/TArray.h"
#include "Engine/Assets/StaticMesh.h"

class UMeshComponent : public UPrimitiveComponent
{
public:
	virtual FStaticMaterial* GetMaterial(uint32 slotIndex) const;
	virtual uint32 GetNumMaterial() const;
	void SetMaterial(uint32 SlotIndex, FStaticMaterial* InMaterial);

private:
	DECLARE_OBJECT(UMeshComponent,UPrimitiveComponent)
	DECLARE_SERIALIZATION()

protected:
	TArray<FStaticMaterial*> OverrideMaterials;

};
