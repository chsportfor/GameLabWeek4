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

struct FCameraData
{
	FVector Location;
	FRotator Rotation;
	float FOV;
	float NearClip;
	float FarClip;

	FCameraData();
	FCameraData(json::JSON);

	json::JSON ToJson() const;
	FString ToJsonString() const;
};

struct FViewportCameraData {
	int32 ViewportIndex = -1;
	FCameraData Camera;
};


