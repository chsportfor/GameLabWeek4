#pragma once

#include <filesystem>
#include "AssetImporter.h"
class URenderer;
struct FTexture2D_uasset;

class FTexture2DImporter : private FAssetImporter
{
    friend class FMaterialImporter;
    friend class FFontAtlasImporter;
    // Prepare without writing so the material importer can commit a whole MTL as one batch.
    static FTexture2D_uasset PrepareTexture2D(URenderer& Renderer,
        const std::filesystem::path& SourcePath, bool bStandalone, bool bGenerateMipMaps = true);
public:
    // Relative paths are resolved under FFileManager's asset root.
    // Destination may be a directory or an explicit .uasset file path.
    // Default: Assets/Textures/<source stem>.uasset. Existing files are not overwritten.
    static TArray<FName> ImportUTexture2D(URenderer& Renderer,
        const std::filesystem::path& SourcePath,
        const std::filesystem::path& Destination = "Textures",
        bool bStandalone = true);
};
