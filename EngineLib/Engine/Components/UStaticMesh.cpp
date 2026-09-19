#include "UStaticMesh.h"

IMPLEMENT_CLASS(UStaticMesh, UObject)


TSharedPtr<FStaticMeshAsset> UStaticMesh::GetStaticMeshAsset() const // StaticMeshAsset ptr 가져옴
{
	return StaticMeshAsset ? StaticMeshAsset : nullptr;
}

FName UStaticMesh::GetAssetName() const // AssetPath FString으로 가져옴
{
	return StaticMeshAsset ? StaticMeshAsset->GetName() : FString{};
}

void UStaticMesh::SetStaticMeshAsset(TSharedPtr<FStaticMeshAsset> InAsset) // 다른 StaticMeshAsset ptr 설정
{
	StaticMeshAsset = InAsset;
}

uint32 UStaticMesh::GetNumMaterial() const // Material 총 Slot 개수 가져옴
{
	return StaticMeshAsset ? StaticMeshAsset->GetMaterials().Num() : 0;
}

const FStaticMeshAssetMaterial* UStaticMesh::GetMaterial(int32 SlotIndex) const // 해당 Slot Index에 해당하는 Material 가져옴
{
	if (StaticMeshAsset && SlotIndex >= 0 && SlotIndex < StaticMeshAsset->GetMaterials().Num())
	{
		return &StaticMeshAsset->GetMaterials()[SlotIndex];
	}
	else
		return nullptr;
}

uint32 UStaticMesh::GetNumSections() const // 총 Section 개수 가져옴
{
	return StaticMeshAsset ? StaticMeshAsset->GetSections().Num() : 0;
}

const FStaticMeshAssetSection* UStaticMesh::GetSections(int32 SectionIndex) const  // 해당 Scetion Index에 해당하는 Section 가져옴
{
	if (StaticMeshAsset && SectionIndex >= 0  && SectionIndex < StaticMeshAsset->GetSections().Num())
	{
		return &StaticMeshAsset->GetSections()[SectionIndex];
	}
	else
		return nullptr;
}
