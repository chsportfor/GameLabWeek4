#pragma once

#include "Engine/Assets/StaticMesh.h"
#include "Core/Object/Object.h"

class UStaticMesh : public UObject
{
	FStaticMesh* StaticMeshAsset;

	const FString& GetAssetPathFileName();
	void SetStaticMeshAsset(FStaticMesh* InStaticMesh);
	FStaticMesh* GetStaticMeshAsset();
	uint32 GetNumMaterial();
	FStaticMaterial GetMaterial(uint32 MaterialIndex);
	uint32 GetNumSections();
	FStaticMeshSection GetSections(uint32 SectionIndex);
};
