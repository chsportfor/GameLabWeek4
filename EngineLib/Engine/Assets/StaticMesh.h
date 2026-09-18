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

struct FStaticMeshSection
{
	FString MaterialName;
	uint32 FirstIndex = 0;
	uint32 NumIndices = 0;
};

struct FStaticMesh
{
	FString PathFileName;
	TArray<FVertexPNCT> Vertices;
	TArray<uint32> Indices;
	TArray<FStaticMeshSection> Sections;
};
