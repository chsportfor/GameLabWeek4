#pragma once

#include "AssetImporter.h"
#include "Rendering/FontResource.h"

class URenderer;
struct FFontAtlas_uasset;

class FFontAtlasImporter : private FAssetImporter
{
public:
    // MSDF image + msdf-atlas-gen JSON (type=msdf, yOrigin=top).
    // Writes one Fonts/<image stem>.uasset by default; no texture asset is created.
    static bool ImportUFontAtlas(URenderer& Renderer,
        const std::filesystem::path& ImagePath, const std::filesystem::path& MetadataPath,
        const std::filesystem::path& Destination = "Fonts", bool bStandalone = true);

    // Bitmap grid atlas. PNG/WIC input keeps one mip; DDS preserves authored mipmaps.
    static bool ImportUFontAtlas(URenderer& Renderer,
        const std::filesystem::path& ImagePath, const FBitmapFontAtlasSettings& Settings,
        const std::filesystem::path& Destination = "Fonts", bool bStandalone = true);

private:
    static bool Import(URenderer& Renderer, const std::filesystem::path& ImagePath,
        const std::filesystem::path& Destination, FFontAtlas_uasset File);
};
