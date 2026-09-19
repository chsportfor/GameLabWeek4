#pragma once

#include <span>

#include "Core/Object/PropertyInfo.h"
#include "UMeshComponent.h"
#include "Core/IO/JsonUtil.h"
#include "Core/Object/ObjectFactory.h"
#include "Engine/Components/UStaticMesh.h"
#include "Core/Object/ObjectIterator.h"

class UStaticMeshComponent : public UMeshComponent
{

	DECLARE_OBJECT(UStaticMeshComponent, UMeshComponent)
	DECLARE_SERIALIZATION()

public:
	void Initialize(FVector Location,FRotator Rotation,FVector Scale);
	static std::span<const FPropertyInfo> GetDeclaredProperties();
	const FStaticMeshAssetMaterial* GetMaterial(int32 slotIndex) const override;
	int32 GetNumMaterial() const override;
	void SetStaticMesh(UStaticMesh* InStaticMesh);
	UStaticMesh* GetStaticMesh() const; 
	void SubmitRenderInfos(FRenderCollector& Collector) const;
	

protected:
	UStaticMesh* StaticMesh = nullptr;
	FName ObjAssetName;
};
