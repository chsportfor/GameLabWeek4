#pragma once

#include <span>

#include "Core/Object/PropertyInfo.h"
#include "UMeshComponent.h"
#include "Core/IO/JsonUtil.h"
#include "Core/Object/ObjectFactory.h"
#include "Engine/Components/UStaticMesh.h"


class UStaticMeshComponent : public UMeshComponent
{

	DECLARE_OBJECT(UStaticMeshComponent, UMeshComponent)
	DECLARE_SERIALIZATION()

public:
	static std::span<const FPropertyInfo> GetDeclaredProperties();
	FStaticMaterial* GetMaterial(uint32 slotIndex) const override;
	uint32 GetNumMaterial() const override;
	void SubmitRenderInfos(FRenderCollector& Collector);
	

protected:
	UStaticMesh* StaticMesh = nullptr;
	FString ObjStaticMeshAsset = "";

	
};
