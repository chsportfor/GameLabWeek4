#include "UStaticMesh.h"

IMPLEMENT_CLASS(UStaticMesh, UObject)


FStaticMesh* UStaticMesh::GetStaticMeshAsset() const
{
	return StaticMeshAsset ? StaticMeshAsset : nullptr;
}

FString UStaticMesh::GetAssetPathFileName() const{
	return StaticMeshAsset ? StaticMeshAsset->PathFileName : FString{};
}

void UStaticMesh::SetStaticMeshAsset(FStaticMesh* InStaticMesh)
{
	StaticMeshAsset = InStaticMesh;
}

uint32 UStaticMesh::GetNumMaterial() const
{
	return StaticMeshAsset ? StaticMeshAsset->Materials.Num() : 0;
}

FStaticMaterial* UStaticMesh::GetMaterial(uint32 MaterialIndex) const
{
	if (StaticMeshAsset && MaterialIndex < StaticMeshAsset->Materials.Num())
	{
		return &StaticMeshAsset->Materials[MaterialIndex];
	}
	else
		return nullptr;
}

uint32 UStaticMesh::GetNumSections() const
{
	return StaticMeshAsset ? StaticMeshAsset->Sections.Num() : 0;
}

FStaticMeshSection* UStaticMesh::GetSections(uint32 SectionIndex) const
{
	if (StaticMeshAsset && SectionIndex < StaticMeshAsset->Sections.Num())
	{
		return &StaticMeshAsset->Sections[SectionIndex];
	}
	else
		return nullptr;
}
