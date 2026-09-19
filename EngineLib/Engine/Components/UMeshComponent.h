#pragma once

#include "PrimitiveComponent.h"
#include "Core/Object/ObjectFactory.h"
#include "Core/Container/TArray.h"
#include "Engine/Assets/StaticMesh.h"

class UMeshComponent : public UPrimitiveComponent
{
public:

	virtual FStaticMaterial* GetMaterial(int32 slotIndex) const;
	virtual int32 GetNumMaterial() const;
	int32 GetNumOverrideMaterial() const;
	void SetMaterial(int32 SlotIndex, FStaticMaterial* InMaterial);

private:

	DECLARE_OBJECT(UMeshComponent,UPrimitiveComponent)
	DECLARE_SERIALIZATION()

protected:

	TArray<FStaticMaterial*> OverrideMaterials;

};
