#pragma once

#include "Engine/Assets/StaticMesh.h"
#include "Core/Object/Object.h"

class UStaticMesh : public UObject
{
	FStaticMesh* StaticMeshAsset;
	
	const FString& GetAssetPathFileName() {
		return StaticMeshAsset->PathFileName;
	}

	void SetStaticMeshAsset(FStaticMesh* InStaticMesh) {
		StaticMeshAsset = InStaticMesh;
	}
};
