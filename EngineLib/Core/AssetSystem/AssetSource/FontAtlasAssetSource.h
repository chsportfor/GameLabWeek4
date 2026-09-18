#pragma once

#include "FileAssetSource.h"
#include <optional>

struct FBitmapFontAtlasSettings
{
    int32 Columns = 16;
    int32 Rows = 16;
    float CharacterWidth = 0.1f;
    float CharacterHeight = 0.2f;
    float CharacterAdvance = 0.7f;
};

// A font atlas is either a bitmap grid or an MSDF image plus its JSON metrics.
class FFontAtlasAssetSource : public FAssetSource
{
public:
    FFontAtlasAssetSource(FFileManager& Files, const std::filesystem::path& TexturePath,
        FBitmapFontAtlasSettings InSettings = {})
        : TextureSource(Files, TexturePath), Settings(InSettings) {}

    FFontAtlasAssetSource(FFileManager& Files, const std::filesystem::path& TexturePath,
        const std::filesystem::path& JsonPath)
        : TextureSource(Files, TexturePath), MetadataSource(std::in_place, Files, JsonPath) {}

    FFileAssetSource TextureSource;
    std::optional<FFileAssetSource> MetadataSource;
    FBitmapFontAtlasSettings Settings;
};
