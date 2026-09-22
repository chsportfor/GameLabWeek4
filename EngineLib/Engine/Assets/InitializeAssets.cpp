#include "InitializeAssets.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/IO/FileManager.h"
#include "Engine/Assets/Importers/StaticMeshImporter.h"
#include <stdexcept>

FName ImportStaticMeshObjAsset(const std::filesystem::path& SourcePath,
    UAssetManager& Assets, URenderer& Renderer, FFileManager& Files)
{
    // Importing twice creates another independent asset rather than reusing raw source files.
    const auto Source = Files.ResolvePath(SourcePath);
    auto Destination = std::filesystem::path("StaticMeshes") / Source.stem();
    auto Candidate = Destination;
    Candidate += ".uasset";
    for (uint32 Suffix = 1; std::filesystem::exists(Files.ResolvePath(Candidate)) ||
        std::filesystem::exists(Files.ResolvePath(Candidate.parent_path() / Candidate.stem())); ++Suffix)
    {
        Candidate = Destination;
        Candidate += "_" + std::to_string(Suffix) + ".uasset";
    }
    if (!FStaticMeshImporter::ImportUStaticMesh(Renderer, Source, Candidate))
        throw std::runtime_error("Static mesh import failed");
    if (!Assets.ScanAssets()) throw std::runtime_error("Imported files were written, but registration failed");
    return UAssetManager::MakeFileAssetName(Candidate, Files);
}
