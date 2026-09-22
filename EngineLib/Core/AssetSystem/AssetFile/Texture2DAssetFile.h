#pragma once

#include "AssetFile.h"
#include "AssetFileSchema.h"
#include <span>

/* UTexture2D disk schema 1.
 * Full file (no padding):
 *   "UAJS"[4] | uint32 LE ContainerVersion=1 | uint64 LE H | uint64 LE B
 *   | UTF-8 HeaderJSON[H] | UTF-8 BodyJSON[B] | BinaryPayload[to EOF]
 * H/B are byte lengths, no NUL. Payload offsets are relative to 24+H+B.
 * See AssetFile.h for shared limits, error and compatibility rules.
 * 
 * HeaderJSON (all required):
 *   {"AssetType":"UTexture2D","SchemaVersion":1,"Standalone":true,"Dependencies":[]}
 * 
 * BodyJSON (Image and all its fields required):
 *   {"Image":{"Encoding":"DDS","Offset":0,"ByteLength":N}}
 * 
 * BinaryPayload: exactly N bytes of a complete DDS file (header + all mipmaps).
 * Encoding must be DDS, Offset must be zero; no trailing bytes/dependencies.
 * Width/Height/MipLevels/Format are derived from DDS. Texture/SRV are recreated.
 */
struct FTexture2D_uasset : public FFile_uasset
{
    FTexture2D_uasset() { AssetType = FString("UTexture2D"); }
    TArray<uint8> Data; // Complete DDS file bytes, including its header and mip chain.
};

namespace AssetFile
{
    const FAssetFileSchema& GetTexture2DFileSchema();
    void UpgradeTexture2DToLatest(FAssetFileDocument& Document);
    TArray<uint8> Serialize(const FTexture2D_uasset& File);
    FTexture2D_uasset DeserializeTexture2D(std::span<const uint8> Bytes);
}
