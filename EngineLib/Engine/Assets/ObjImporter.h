#pragma once

#include <string_view>

#include "Core/Container/TArray.h"
#include "Core/Core.h"
#include "Core/IO/FileManager.h"
#include "Core/Math/Vector.h"
#include "Engine/Assets/StaticMesh.h"

struct FObjVertexIndex
{
	int32 PositionIndex = -1;
	int32 UVIndex = -1;
	int32 NormalIndex = -1;
};

struct FObjFace
{
	TArray<FObjVertexIndex> Vertices;
	FString MaterialName;
	uint32 LineNumber = 0;
};

struct FObjInfo
{
	TArray<FVector> Positions;
	TArray<FVector2> UVs;
	TArray<FVector> Normals;
	TArray<FObjFace> Faces;
};

class FObjImporter
{
public:
	static bool Parse(std::string_view objText, FStaticMesh& outMesh, FString& outError);
	static bool LoadFromFile(std::string_view path, const FFileManager& fileManager,
		FStaticMesh& outMesh, FString& outError);
};
