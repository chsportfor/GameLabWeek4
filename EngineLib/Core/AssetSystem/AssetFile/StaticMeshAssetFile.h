#pragma once

#include "AssetFile.h"
#include "AssetFileSchema.h"
#include "Core/AssetSystem/Asset/MeshGeometry.h"
#include "Core/Math/FBoundingBox.h"

/* UStaticMeshAsset disk schema 1.
 * Full file (no padding):
 *   "UAJS"[4] | uint32 LE ContainerVersion=1 | uint64 LE H | uint64 LE B
 *   | UTF-8 HeaderJSON[H] | UTF-8 BodyJSON[B] | BinaryPayload[to EOF]
 * See AssetFile.h for shared limits, error and compatibility rules.
 * 
 * HeaderJSON (all required):
 *   {"AssetType":"UStaticMeshAsset","SchemaVersion":1,"Standalone":true,
 *    "Dependencies":["Materials/Body.uasset"]}
 * 
 * BodyJSON (all shown fields required):
 *   {"Bounds":{"Min":[x,y,z],"Max":[x,y,z]},
 *    "MaterialPaths":["Materials/Body.uasset"],
 *    "Sections":[{"FirstIndex":0,"IndexCount":I,"MaterialIndex":0}],
 *    "Geometry":{"VertexLayout":"VertexSimpleV1","IndexFormat":"UInt32",
 *      "Vertices":{"Offset":0,"Count":V,"ByteLength":48*V},
 *      "Indices":{"Offset":48*V,"Count":I,"ByteLength":4*I}}}
 * 
 * BinaryPayload: V vertices followed immediately by I indices, no trailing bytes.
 */
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
    const FAssetFileSchema& GetStaticMeshFileSchema();
    void UpgradeStaticMeshToLatest(FAssetFileDocument& Document);
    TArray<uint8> Serialize(const FStaticMesh_uasset& File);
    FStaticMesh_uasset DeserializeStaticMesh(std::span<const uint8> Bytes);
}
