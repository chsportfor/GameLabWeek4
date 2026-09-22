#pragma once

#include "AssetFile.h"
#include <span>

struct FTexture2D_uasset : public FFile_uasset
{
    FTexture2D_uasset() { AssetType = FString("UTexture2D"); }
    TArray<uint8> Data; // Complete DDS file bytes, including its header and mip chain.
};

namespace AssetFile
{
    // Version 1, little endian: UAST/version/type/standalone/dependencies/data size/data.
    // Strings are uint32 byte length + UTF-8 bytes, data size is uint64.
    // Throws on malformed data or unsupported versions/types.
    TArray<uint8> Serialize(const FTexture2D_uasset& File);
    FTexture2D_uasset DeserializeTexture2D(std::span<const uint8> Bytes);
}
