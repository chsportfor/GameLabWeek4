#pragma once

#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/Object/Object.h"

class UStaticMesh : public UObject
{
	DECLARE_OBJECT(UStaticMesh, UObject)

public:
	TSharedPtr<FStaticMeshAsset> StaticMeshAsset = nullptr; 

	FName GetAssetName() const;
	void SetStaticMeshAsset(TSharedPtr<FStaticMeshAsset> InAsset);
	TSharedPtr<FStaticMeshAsset> GetStaticMeshAsset() const;
	uint32 GetNumMaterial() const;
	const FStaticMeshAssetMaterial* GetMaterial(int32 SlotIndex) const;
	uint32 GetNumSections() const;
	const FStaticMeshAssetSection* GetSections(int32 SectionIndex) const;
};
