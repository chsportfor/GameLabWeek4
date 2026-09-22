#pragma once

#include "AssetFile.h"
#include "AssetFileSchema.h"
#include "Rendering/FontResource.h"

// A self-contained font: the atlas image is embedded, not a UTexture2D dependency.
/* UFontAtlasAsset disk schema 1; atlas image belongs to this asset, not a dependency.
 * Full file (no padding):
 *   "UAJS"[4] | uint32 LE ContainerVersion=1 | uint64 LE H | uint64 LE B
 *   | UTF-8 HeaderJSON[H] | UTF-8 BodyJSON[B] | BinaryPayload[to EOF]
 * H/B are byte lengths, no NUL. Payload offsets are relative to 24+H+B.
 * See AssetFile.h for shared limits, error and compatibility rules.
 * HeaderJSON (all required):
 *   {"AssetType":"UFontAtlasAsset","SchemaVersion":1,"Standalone":true,"Dependencies":[]}
 * Bitmap BodyJSON:
 *   {"Mode":"Bitmap","BitmapSettings":{"Columns":16,"Rows":16,
 *      "CharacterWidth":0.1,"CharacterHeight":0.2,"CharacterAdvance":0.7},
 *    "Image":{"Encoding":"DDS","Offset":0,"ByteLength":N}}
 * MSDF BodyJSON:
 *   {"Mode":"MSDF","Metadata":{
 *      "atlas":{"type":"msdf","width":W,"height":H,"distanceRange":4,"yOrigin":"top"},
 *      "glyphs":[{"unicode":65,"advance":0.7,
 *        "planeBounds":{"left":0,"top":-0.8,"right":0.6,"bottom":0},
 *        "atlasBounds":{"left":0,"top":0,"right":32,"bottom":40}}]},
 *    "Image":{"Encoding":"DDS","Offset":0,"ByteLength":N}}
 * W/H/N above represent actual numeric values; H here is atlas height.
 * Metadata is a JSON object, not escaped JSON text; imported additional fields
 * (e.g. metrics) are preserved. Glyphs without geometry omit BOTH bounds objects.
 * Mode/Image and all image descriptor fields required. Mode is Bitmap or MSDF.
 * BitmapSettings may be absent; omitted fields retain the defaults shown above.
 * Columns/Rows >0, <=256, product <=256; width/height >0, advance >=0, all finite.
 * For MSDF, Metadata and the atlas/glyph fields consumed by FFontResource are
 * required: type=msdf, yOrigin=top, positive integer dimensions <=16384,
 * positive distanceRange, nonempty glyphs with unique valid Unicode scalars,
 * finite advance; paired bounds must have positive area and atlas bounds fit.
 * BinaryPayload: exactly N bytes of complete DDS; Offset=0, no trailing bytes.
 * Dependencies must be empty. Texture/SRV and character lookup maps are rebuilt.
 */
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
    const FAssetFileSchema& GetFontAtlasFileSchema();
    void UpgradeFontAtlasToLatest(FAssetFileDocument& Document);
    TArray<uint8> Serialize(const FFontAtlas_uasset& File);
    FFontAtlas_uasset DeserializeFontAtlas(std::span<const uint8> Bytes);
}
