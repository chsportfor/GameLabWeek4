#pragma once

#include "AssetFile.h"
#include "Rendering/FontResource.h"

// A self-contained font: the atlas image is embedded, not a UTexture2D dependency.
struct FFontAtlas_uasset : public FFile_uasset
{
    FFontAtlas_uasset() { AssetType = FString("UFontAtlasAsset"); }

    TArray<uint8> Data; // Complete DDS bytes.
    bool bMSDF = false;
    FString MetadataJson; // MSDF only: embedded glyphs, metrics and atlas settings.
    FBitmapFontAtlasSettings BitmapSettings; // Bitmap only.

    // Validates the font definition and builds the font tables from the embedded settings.
    // Does not create a UObject, access source files, or allocate GPU resources.
    FFontResource MakeFontResource() const;
};

namespace AssetFile
{
    // Common header, uint8 MSDF flag, then either length-prefixed JSON or grid settings,
    // followed by uint64 DDS size and DDS bytes. Integers/floats are little endian.
    TArray<uint8> Serialize(const FFontAtlas_uasset& File);
    FFontAtlas_uasset DeserializeFontAtlas(std::span<const uint8> Bytes);
}
