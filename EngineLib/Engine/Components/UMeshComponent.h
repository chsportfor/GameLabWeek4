#pragma once

#include "PrimitiveComponent.h"
#include "Core/Object/ObjectFactory.h"
#include "Core/Container/TArray.h"
#include "Core/AssetSystem/Asset/Material.h"

class UMeshComponent : public UPrimitiveComponent
{
public:
	static std::span<const FPropertyInfo> GetDeclaredProperties();

	virtual UMaterial* GetMaterial(int32 slotIndex) const;
	virtual int32 GetNumMaterial() const;
	int32 GetNumOverrideMaterial() const;
	void SetMaterial(int32 SlotIndex, UMaterial* InMaterial);
	UMaterial* GetMaterialOverride(int32 SlotIndex) const;
	void ClearMaterialOverride(int32 SlotIndex);

private:

	DECLARE_OBJECT(UMeshComponent,UPrimitiveComponent)
	DECLARE_SERIALIZATION()

protected:

	TArray<UMaterial*> OverrideMaterials;

};
