#include "UStaticMeshComponent.h"
#include "Core/AssetSystem/AssetManager.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(UStaticMeshComponent, UMeshComponent)
IMPLEMENT_SERIALIZATION(
	UStaticMeshComponent,
	UMeshComponent,
	{
		StaticMesh = nullptr;
		for (FObjectIterator<UStaticMesh> It; It; ++It) //기존 UStaticMesh 검색
		{
			if ((*It)->GetAssetName() == ObjAssetName)
			{
			SetStaticMesh(*It);
			break;
			}
		}

		if (!StaticMesh) // 기존 객체가 없으면 AssetManager에서 로드
		{
			FAssetManager* Manager =
			FObjectFactory::GetDefaultAssetManager();
			if (Manager)
			{
				auto Asset = Manager->GetAssetAs<FStaticMeshAsset>(ObjAssetName, true);
				if (Asset)
				{
					UStaticMesh* NewStaticMesh =
					FObjectFactory::ConstructObject<UStaticMesh>();
					NewStaticMesh->SetStaticMeshAsset(Asset);
					SetStaticMesh(NewStaticMesh);
				}
			}
		}
	}
);

const FStaticMeshAssetMaterial* UStaticMeshComponent::GetMaterial(int32 slotIndex) const // OverrideMaterials에 이미 Material 있으면 해당 Slot의 Material을 그걸로 지정, 그게 아니면 원래것으로 지정 -> 최종적으로 슬롯에 넣을 Material 반환
{
	if (slotIndex < 0)
	{
		return nullptr;
	}
	if (slotIndex<OverrideMaterials.Num() && OverrideMaterials[slotIndex] != nullptr) // SlotIndex가 유효하고 OverrideMaterials[SlotIndex]에 값이 있으면 Material을 Override
	{
		return OverrideMaterials[slotIndex];
	}
	return StaticMesh ? StaticMesh->GetMaterial(slotIndex) : nullptr;
}

int32 UStaticMeshComponent::GetNumMaterial() const // Material Slot 개수 가져옴
{
	return StaticMesh ? StaticMesh->GetNumMaterial() : 0;
}

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InStaticMesh) // StaticMesh 설정
{
	StaticMesh = InStaticMesh;
	ObjAssetName =StaticMesh ? StaticMesh->GetAssetName() : FName{};
}

UStaticMesh* UStaticMeshComponent::GetStaticMesh() const // StaticMesh 가져옴
{
	return StaticMesh ? StaticMesh : nullptr;
}

std::span<const FPropertyInfo>
UStaticMeshComponent::GetDeclaredProperties() // Serialization 때 Properties 밑에 추가할 값 설정
{
	static const FPropertyInfo Properties[] =
	{
		REFLECT_PROPERTY(
			UStaticMeshComponent,
			ObjAssetName)
	};
	return Properties;
}

void UStaticMeshComponent::SubmitRenderInfos(FRenderCollector& Collector) const
{
	if (!StaticMesh)
		return;

	TSharedPtr<FStaticMeshAsset> Asset = StaticMesh->GetStaticMeshAsset();

	if (!Asset)
		return;

	const auto Model = GetRenderTransform(Collector.View.Camera);
	const TArray<FStaticMeshAssetSection> Sections = Asset->GetSections();

	FRenderStaticMeshInfo info{};
	for (int i = 0;i < Sections.Num();i++)
	{
		const FStaticMeshAssetSection& Section = Sections[i];

		info.WorldTransformMatrix = Model;
		info.VertexBuffer = StaticMesh->GetStaticMeshAsset()->GetVertexBuffer();
		info.IndexBuffer = StaticMesh->GetStaticMeshAsset()->GetIndexBuffer();
		info.VertexCount = StaticMesh->GetStaticMeshAsset()->GetVertexCount();
		info.FirstIndex = Section.FirstIndex;
		info.IndexCount = Section.IndexCount;
		info.Color = GetMaterial(Section.MaterialIndex)->DiffuseColor;
		info.Texture = GetMaterial(Section.MaterialIndex)->DiffuseTexture;
		Collector.StaticMeshInfos.Add(info);
	}
}

