#pragma once

#include "Core/Core.h"
#include "Core/Container/TArray.h"
#include "Core/Name.h"
#include <filesystem>
#include <span>

// ImportU* returns only newly written assets as asset-root-relative .uasset names.
// Failure is logged with UE_LOG and returns an empty array; prepared outputs are rolled back.
// Existing dependencies that are only referenced are not included.
class FAssetImporter
{
protected:
    struct FPreparedAssetFile
    {
        std::filesystem::path Path;
        TArray<uint8> Bytes;
    };
    struct FAssetFileToWrite
    {
        std::filesystem::path Path;
        std::span<const uint8> Bytes;
    };
    // Creates missing directories; never overwrites an existing asset.
    static TArray<FName> WriteImportedAsset(const std::filesystem::path& AssetPath, std::span<const uint8> Bytes);
    // Commit related outputs together. On failure remove only files created by this call.
    static TArray<FName> WriteImportedAssets(std::span<const FAssetFileToWrite> Files);
};
