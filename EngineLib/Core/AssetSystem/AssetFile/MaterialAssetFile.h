#pragma once

#include "AssetFile.h"
#include "Core/Math/Color.h"

struct FMaterial_uasset : public FFile_uasset
{
    FMaterial_uasset() { AssetType = FString("UMaterial"); }
    FLinearColor DiffuseColor{1, 1, 1, 1};
    // UTF-8, relative to the asset root, including .uasset. Empty means no texture.
    FString DiffuseTexturePath;
};

namespace AssetFile
{
    // Body: four little-endian float32 values, uint32 path byte length, UTF-8 path.
    // Header dependencies are derived from the body's references when saving.
    TArray<uint8> Serialize(const FMaterial_uasset& File);
    FMaterial_uasset DeserializeMaterial(std::span<const uint8> Bytes);
}
