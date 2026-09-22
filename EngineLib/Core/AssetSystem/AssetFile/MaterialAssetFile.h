#pragma once

#include "AssetFile.h"
#include "AssetFileSchema.h"
#include "Core/Math/Color.h"

/* UMaterial disk schema 1.
 * Full file (no padding):
 *   "UAJS"[4] | uint32 LE ContainerVersion=1 | uint64 LE H | uint64 LE B
 *   | UTF-8 HeaderJSON[H] | UTF-8 BodyJSON[B] | BinaryPayload[to EOF]

 * HeaderJSON (all required):
 *   {"AssetType":"UMaterial","SchemaVersion":1,"Standalone":true,
 *    "Dependencies":["Textures/Body.uasset"]}
 * 
 * BodyJSON:
 *   {"DiffuseColor":[R,G,B,A],"DiffuseTexture":"Textures/Body.uasset"}
 * 
 * BinaryPayload: empty (EOF immediately after BodyJSON).
 */
struct FMaterial_uasset : public FFile_uasset
{
    FMaterial_uasset() { AssetType = FString("UMaterial"); }
    FLinearColor DiffuseColor{1, 1, 1, 1};
    // UTF-8, relative to the asset root, including .uasset. Empty means no texture.
    FString DiffuseTexturePath;
};

namespace AssetFile
{
    const FAssetFileSchema& GetMaterialFileSchema();
    void UpgradeMaterialToLatest(FAssetFileDocument& Document);
    TArray<uint8> Serialize(const FMaterial_uasset& File);
    FMaterial_uasset DeserializeMaterial(std::span<const uint8> Bytes);
}
