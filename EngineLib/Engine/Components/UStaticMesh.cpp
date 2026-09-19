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

uint32 UStaticMesh::GetNumMaterial()
{
	return StaticMeshAsset->Materials.Num();
}

FStaticMaterial UStaticMesh::GetMaterial(uint32 MaterialIndex)
{
	return StaticMeshAsset->Materials[MaterialIndex];
}

uint32 UStaticMesh::GetNumSections()
{
	return StaticMeshAsset->Sections.Num();
}

FStaticMeshSection UStaticMesh::GetSections(uint32 SectionIndex)
{
	return StaticMeshAsset->Sections[SectionIndex];
}
