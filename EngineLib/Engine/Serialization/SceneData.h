#pragma once

//#include "Json/json.hpp"

#include "Core/Core.h"
#include "Core/Container/TArray.h"
#include "Core/Container/TMap.h"
#include "Core/Math/Vector.h"
#include "Core/Math/Rotator.h"
#include "Core/enum.h"

namespace json
{
	class JSON;
}

struct FPrimitiveData
{
	FVector Location;
	FRotator Rotation;
	FVector Scale;
	EPrimitive PrimitiveType;

	FPrimitiveData();
	FPrimitiveData(json::JSON);

	json::JSON ToJson() const;
	FString ToJsonString() const;
};

struct FSceneData
{
	uint32 Version;
	uint32 NextUUID;
	TMap<uint32, FPrimitiveData> Primitives;

	FSceneData();
	FSceneData(json::JSON);

	json::JSON ToJson() const;
	FString ToJsonString() const;
};


