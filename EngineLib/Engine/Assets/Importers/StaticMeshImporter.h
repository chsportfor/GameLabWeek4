#pragma once

#include "AssetImporter.h"

class URenderer;
struct FStaticMesh;

class FStaticMeshImporter : private FAssetImporter
{
    static bool ImportParsedMesh(URenderer& Renderer, const FStaticMesh& Mesh,
        const std::filesystem::path& SourceStem, const std::filesystem::path& Destination, bool bStandalone);
public:
    // Destination: directory or explicit .uasset filename, relative to the asset root if not absolute.
    // Materials and textures share <mesh asset directory>/<final mesh asset stem>/.
    static bool ImportUStaticMesh(URenderer& Renderer, const std::filesystem::path& ObjPath,
        const std::filesystem::path& Destination = "StaticMeshes", bool bStandalone = true);

    // Imports the PODOMSH v2 .pmesh cache. Original OBJ/MTL must still match the cache.
    static bool ImportUStaticMeshFromBinary(URenderer& Renderer, const std::filesystem::path& BinaryPath,
        const std::filesystem::path& Destination = "StaticMeshes", bool bStandalone = true);
};
