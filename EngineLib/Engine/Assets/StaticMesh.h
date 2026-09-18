#pragma once

#include "Core/Container/TArray.h"
#include "Core/Core.h"
#include "Core/Math/Vector.h"

struct FVertexPNCT
{
	FVector Position;
	FVector Normal;
	FVector4 Color;
	FVector2 UV;
};

struct FStaticMesh
{
	FString PathFileName;
	TArray<FVertexPNCT> Vertices;
	TArray<uint32> Indices;
};
