#pragma once

#include "Core/AssetSystem/Asset.h"
#include "Core/IO/FileManager.h"

class FFileAssetSource : public FAssetSource
{
public:
    FFileAssetSource(FFileManager& InFileManager, const std::filesystem::path& InFilePath)
        : FileManager(InFileManager), FilePath(InFilePath) {}

    FString ReadFileToString() const { return FileManager.ReadFileToString(FilePath); }
    void SetFilePath(const std::filesystem::path& InFilePath) { FilePath = InFilePath; }

    FFileManager& FileManager;
    std::filesystem::path FilePath;
};
