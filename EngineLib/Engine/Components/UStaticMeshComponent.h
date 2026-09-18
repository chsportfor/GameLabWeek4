#pragma once

#include "UMeshComponent.h"
#include "Core/IO/JsonUtil.h"
#include "Core/Object/ObjectFactory.h"

class UStaticMeshComponent : public UMeshComponent
{
	DECLARE_OBJECT(UStaticMeshComponent, UMeshComponent)
	DECLARE_SERIALIZATION()

	//UStaticMesh* StaticMesh; 
	void Serialize(bool bIsLoading, json::JSON Handle);
};
