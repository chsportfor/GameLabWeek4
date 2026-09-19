#pragma once

#include "PrimitiveComponent.h"
#include "Core/Object/ObjectFactory.h"
#include "Core/Container/TArray.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"

class UMeshComponent : public UPrimitiveComponent
{
public:

	virtual const FStaticMeshAssetMaterial* GetMaterial(int32 slotIndex) const;
	virtual int32 GetNumMaterial() const;
	int32 GetNumOverrideMaterial() const;
	void SetMaterial(int32 SlotIndex, const FStaticMeshAssetMaterial* InMaterial);

private:

	DECLARE_OBJECT(UMeshComponent,UPrimitiveComponent)
	DECLARE_SERIALIZATION()

protected:

	TArray<const FStaticMeshAssetMaterial*> OverrideMaterials;

};
