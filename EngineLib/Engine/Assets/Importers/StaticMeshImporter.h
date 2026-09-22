#pragma once

#include "AssetImporter.h"

class URenderer;
struct FStaticMesh;

class FStaticMeshImporter : private FAssetImporter
{
    static TArray<FName> ImportParsedMesh(URenderer& Renderer, const FStaticMesh& Mesh,
        const std::filesystem::path& SourceStem, const std::filesystem::path& Destination,
        bool bStandalone, bool bFlipTextureV);
public:
    // Result[0] is the mesh; remaining names are newly created dependency assets.
    // Destination: directory or explicit .uasset filename, relative to the asset root if not absolute.
    // Materials and textures use Materials/ and Textures/ below the mesh dependency directory.
    static TArray<FName> ImportUStaticMesh(URenderer& Renderer, const std::filesystem::path& ObjPath,
        const std::filesystem::path& Destination = "StaticMeshes", bool bStandalone = true);

    static TArray<FName> ImportUStaticMesh(URenderer& Renderer, const FStaticMesh& Mesh,
        const std::filesystem::path& SourceStem, const std::filesystem::path& Destination,
        bool bStandalone = true, bool bFlipTextureV = false);

    // Imports the PODOMSH v2 .pmesh cache. Original OBJ/MTL must still match the cache.
    static TArray<FName> ImportUStaticMeshFromBinary(URenderer& Renderer, const std::filesystem::path& BinaryPath,
        const std::filesystem::path& Destination = "StaticMeshes", bool bStandalone = true);
};
