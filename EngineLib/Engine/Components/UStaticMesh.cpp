#include "UStaticMesh.h"

FStaticMesh* UStaticMesh::GetStaticMeshAsset()
{
	return StaticMeshAsset;
}

const FString& UStaticMesh::GetAssetPathFileName() {
	return StaticMeshAsset->PathFileName;
}

void UStaticMesh::SetStaticMeshAsset(FStaticMesh* InStaticMesh)
{
	StaticMeshAsset = InStaticMesh;
}

FStaticMesh* UStaticMesh::GetStaticMeshAsset()
{
	return StaticMeshAsset;
}

FStaticMaterial UStaticMesh::GetMaterial(uint32 MaterialIndex)
{
	return StaticMeshAsset->Materials[MaterialIndex];
}
