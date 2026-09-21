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
	uint32 MaterialIndex = 0xFFFFFFFFu;
	uint32 FirstIndex = 0;
	uint32 NumIndices = 0;
};

struct FStaticMeshIndexRange
{
	uint32 MaterialIndex = 0xFFFFFFFFu;
	uint32 FirstIndex = 0;
	uint32 NumIndices = 0;
};

struct FStaticMeshPart
{
	FString ObjectName;
	TArray<FString> GroupNames;
	TArray<FStaticMeshIndexRange> IndexRanges;
};

struct FStaticMaterial
{
	FString Name;
	FString MaterialLibraryPath;
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

struct FStaticMesh
{
	FString PathFileName;
	TArray<FVertexPNCT> Vertices;
	TArray<uint32> Indices;
	TArray<FStaticMeshSection> Sections;
	TArray<FStaticMeshPart> Parts;
	// One entry per cooked triangle, in the same order as Indices / 3.
	TArray<int32> TriangleSmoothingGroups;
	TArray<FStaticMaterial> Materials;
};

