#pragma once
#include "Core/AssetSystem/AssetFile/AssetFile.h"
#include "Core/Name.h"
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

class FFileManager;
class UAssetManager;

// Read-only disk inspection. Neither selection nor drawing loads a UObject/GPU resource.
class FAssetPropertyPanel
{
public:
    void Inspect(const std::filesystem::path& RelativePath, const FFileManager& Files);
    void Refresh(const FFileManager& Files) { Inspect(Path, Files); }
    bool ValidateSelection(const UAssetManager& Assets);
    void Draw(const FFileManager& Files, UAssetManager& Assets);
private:
    std::filesystem::path Path;
    FName Name;
    FFile_uasset Header;
    uint64 FileBytes = 0;
    std::string Error;
    std::vector<std::pair<std::string, std::string>> Details;
};
