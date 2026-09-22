#pragma once

#include "AssetFile.h"
#include "Core/AssetSystem/Asset/MeshGeometry.h"
#include "Core/Math/FBoundingBox.h"

struct FStaticMesh_uasset : public FFile_uasset
{
    FStaticMesh_uasset() { AssetType = FString("UStaticMeshAsset"); }
    FMeshGeometry Geometry;
    FBoundingBox Bounds;
    TArray<FMeshSection> Sections;
    // Ordered material slots; duplicates are allowed. UTF-8 asset-root-relative .uasset paths.
    TArray<FString> MaterialPaths;
};

namespace AssetFile
{
    // Explicit little-endian fields, never a raw dump of a C++ struct.
    // Dependencies are derived from MaterialPaths, removing duplicates only in the header.
    TArray<uint8> Serialize(const FStaticMesh_uasset& File);
    FStaticMesh_uasset DeserializeStaticMesh(std::span<const uint8> Bytes);
}
