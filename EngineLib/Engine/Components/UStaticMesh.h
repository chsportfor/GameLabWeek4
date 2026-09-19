#pragma once

#include "Engine/Assets/StaticMesh.h"
#include "Core/Object/Object.h"

class UStaticMesh : public UObject
{
	DECLARE_OBJECT(UStaticMesh, UObject)

public:
	FStaticMesh* StaticMeshAsset = nullptr; // 후에 AssetManager 완료시 TSharedPtr<FStaticMeshAsset>로 교체될 예정

	FString GetAssetPathFileName() const;
	void SetStaticMeshAsset(FStaticMesh* InStaticMesh);
	FStaticMesh* GetStaticMeshAsset() const;
	uint32 GetNumMaterial() const;
	FStaticMaterial* GetMaterial(uint32 MaterialIndex) const;
	uint32 GetNumSections() const;
	FStaticMeshSection* GetSections(uint32 SectionIndex) const;
};
