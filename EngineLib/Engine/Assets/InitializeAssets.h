#pragma once
#include <filesystem>
class UAssetManager;
class FFileManager;
struct FName;
struct FStaticMesh;
class URenderer;

FName ImportStaticMeshObjAsset(const std::filesystem::path& SourcePath,
    UAssetManager& Assets, URenderer& Renderer, FFileManager& Files);

FName ImportStaticMeshAsset(const FStaticMesh& Mesh, const std::filesystem::path& SourcePath,
    bool bFlipTextureV, UAssetManager& Assets, URenderer& Renderer, FFileManager& Files,
    const std::filesystem::path& Destination = {});
