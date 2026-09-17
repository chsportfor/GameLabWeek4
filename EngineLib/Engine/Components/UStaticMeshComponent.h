#pragma once

#include "UMeshComponent.h"
#include "Core/IO/JsonUtil.h"

class UStaticMeshComponent : public UMeshComponent
{
	//UStaticMesh* StaticMesh; 
	void Serialize(bool bIsLoading, json::JSON Handle);
};
