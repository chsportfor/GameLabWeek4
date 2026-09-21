#pragma once

#include <span>

#include "Core/Object/PropertyInfo.h"
#include "UMeshComponent.h"
#include "Core/IO/JsonUtil.h"
#include "Core/Object/ObjectFactory.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/Object/ObjectIterator.h"

class UStaticMeshComponent : public UMeshComponent
{

	DECLARE_OBJECT(UStaticMeshComponent, UMeshComponent)
	DECLARE_SERIALIZATION()

public:
	void Initialize(FVector Location,FRotator Rotation,FVector Scale);
	static std::span<const FPropertyInfo> GetDeclaredProperties();
	UMaterial* GetMaterial(int32 slotIndex) const override;
	int32 GetNumMaterial() const override;
	void SetStaticMesh(UStaticMeshAsset* InStaticMesh);
	UStaticMeshAsset* GetStaticMesh() const;
	void SubmitRenderInfos(FRenderCollector& Collector) const override;
	void RegisterPickTarget(FPickTargets& Targets) const override;
	bool RayCastComponent(const FPickingRay& Ray, const FCamera& Camera, float& OutHitT) const override;
	

protected:
	FRenderMeshInfo MakeMeshInfo(const FRenderCollector& Collector) const override;
	UStaticMeshAsset* StaticMesh = nullptr;
};
