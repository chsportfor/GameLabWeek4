#pragma once

#include <filesystem>

#include "Core/Pointer/SharedPointer.h"

class FAssetManager;
class FFileManager;
class FFileAssetSource;
struct FName;
class URenderer;

// Startup registration only; no retained assets or lookup tables.
// Files and Renderer must outlive the registered sources/loaders.
void RegisterLoadingScreenAssets(FAssetManager& Assets, URenderer& Renderer, FFileManager& Files);
void RegisterSceneAssets(FAssetManager& Assets, URenderer& Renderer, FFileManager& Files);
FName ImportStaticMeshObjAsset(const std::filesystem::path& SourcePath,
	FAssetManager& Assets, URenderer& Renderer, FFileManager& Files);
TSharedPtr<FFileAssetSource> RegisterObjViewerAssets(
    FAssetManager& Assets, URenderer& Renderer, FFileManager& Files);
