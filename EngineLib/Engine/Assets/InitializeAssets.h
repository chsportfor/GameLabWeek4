#pragma once
#include <filesystem>
class UAssetManager;
class FFileManager;
struct FName;
class URenderer;

FName ImportStaticMeshObjAsset(const std::filesystem::path& SourcePath,
    UAssetManager& Assets, URenderer& Renderer, FFileManager& Files);
