#pragma once

#include <filesystem>

class UAssetManager;
class FFileManager;
struct FName;
class URenderer;

// Startup registration only; no retained assets or lookup tables.
// Files and Renderer must outlive the registered sources/loaders.
void RegisterLoadingScreenAssets(UAssetManager& Assets, URenderer& Renderer, FFileManager& Files);
void RegisterSceneAssets(UAssetManager& Assets, URenderer& Renderer, FFileManager& Files);
FName ImportStaticMeshObjAsset(const std::filesystem::path& SourcePath,
	UAssetManager& Assets, URenderer& Renderer, FFileManager& Files);
FName RegisterObjFileAsset(const std::filesystem::path& Path,
    UAssetManager& Assets, URenderer& Renderer, FFileManager& Files);
