#pragma once

#include "UMeshComponent.h"
#include "Core/IO/JsonUtil.h"
#include "Core/Object/ObjectFactory.h"
#include "Engine/Assets/StaticMesh.h"

class UStaticMeshComponent : public UMeshComponent
{
	DECLARE_OBJECT(UStaticMeshComponent, UMeshComponent)
	DECLARE_SERIALIZATION()

	FStaticMesh* StaticMesh; 
	void Serialize(bool bIsLoading, json::JSON Handle);
};
