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
	FString ObjectName;
	TArray<FString> GroupNames;
	FString MaterialName;
	int32 SmoothingGroup = 0;
	uint32 LineNumber = 0;
};

struct FObjMaterial
{
	FString Name;
	FVector4 AmbientColor = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	FVector4 DiffuseColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	FVector4 SpecularColor = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	FVector4 EmissiveColor = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	FVector4 TransmissionFilter = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float SpecularExponent = 0.0f;
	float OpticalDensity = 1.0f;
	float Dissolve = 1.0f;
	float Transparency = 0.0f;
	int32 IlluminationModel = 0;
	bool bHasDissolve = false;
	bool bHasTransparency = false;
	FString AmbientTexturePath;
	FString DiffuseTexturePath;
	FString SpecularTexturePath;
	FString SpecularExponentTexturePath;
	FString EmissiveTexturePath;
	FString OpacityTexturePath;
	FString NormalTexturePath;
	FString DisplacementTexturePath;
	FString DecalTexturePath;
	FString ReflectionTexturePath;
};

struct FObjInfo
{
	TArray<FVector> Positions;
	TArray<FVector2> UVs;
	TArray<FVector> Normals;
	TArray<FObjFace> Faces;
	TArray<FString> MaterialLibraryPaths;
	TArray<FObjMaterial> Materials;
};

class FObjImporter
{
public:
	static bool Parse(std::string_view objText, FStaticMesh& outMesh, FString& outError);
	static bool LoadFromFile(std::string_view path, const FFileManager& fileManager,
		FStaticMesh& outMesh, FString& outError);
};
