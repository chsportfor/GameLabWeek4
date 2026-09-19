#include "UStaticMesh.h"

IMPLEMENT_CLASS(UStaticMesh, UObject)


FStaticMesh* UStaticMesh::GetStaticMeshAsset() const // StaticMeshAsset ptr 가져옴
{
	return StaticMeshAsset ? StaticMeshAsset : nullptr;
}

FString UStaticMesh::GetAssetPathFileName() const // AssetPath FString으로 가져옴
{
	return StaticMeshAsset ? StaticMeshAsset->PathFileName : FString{};
}

void UStaticMesh::SetStaticMeshAsset(FStaticMesh* InStaticMesh) // 다른 StaticMeshAsset ptr 설정
{
	StaticMeshAsset = InStaticMesh;
}

uint32 UStaticMesh::GetNumMaterial() const // Material 총 Slot 개수 가져옴
{
	return StaticMeshAsset ? StaticMeshAsset->Materials.Num() : 0;
}

FStaticMaterial* UStaticMesh::GetMaterial(uint32 SlotIndex) const // 해당 Slot Index에 해당하는 Material 가져옴
{
	if (StaticMeshAsset && SlotIndex < StaticMeshAsset->Materials.Num())
	{
		return &StaticMeshAsset->Materials[SlotIndex];
	}
	else
		return nullptr;
}

uint32 UStaticMesh::GetNumSections() const // 총 Section 개수 가져옴
{
	return StaticMeshAsset ? StaticMeshAsset->Sections.Num() : 0;
}

FStaticMeshSection* UStaticMesh::GetSections(uint32 SectionIndex) const  // 해당 Scetion Index에 해당하는 Section 가져옴
{
	if (StaticMeshAsset && SectionIndex < StaticMeshAsset->Sections.Num())
	{
		return &StaticMeshAsset->Sections[SectionIndex];
	}
	else
		return nullptr;
}
