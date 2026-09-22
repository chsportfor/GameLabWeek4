#pragma once

#include "Core/Core.h"
#include "Core/Container/TArray.h"
#include <filesystem>
#include <span>

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
    static bool WriteImportedAsset(const std::filesystem::path& AssetPath, std::span<const uint8> Bytes);
    // Commit related outputs together. On failure remove only files created by this call.
    static bool WriteImportedAssets(std::span<const FAssetFileToWrite> Files);
};
