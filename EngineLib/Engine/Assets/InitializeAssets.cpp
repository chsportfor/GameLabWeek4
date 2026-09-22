#include "InitializeAssets.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/IO/FileManager.h"
#include "Engine/Assets/Importers/StaticMeshImporter.h"
#include <stdexcept>

namespace
{
    std::filesystem::path MakeSafeAssetStem(std::filesystem::path Stem)
    {
        std::wstring Name = Stem.wstring();
        while (!Name.empty() && (Name.back() == L' ' || Name.back() == L'.')) Name.pop_back();
        if (Name.empty()) Name = L"ImportedMesh";
        return Name;
    }

    std::filesystem::path MakeUniqueAssetDestination(const std::filesystem::path& Source,
        FFileManager& Files)
    {
        auto Destination = std::filesystem::path("StaticMeshes") / MakeSafeAssetStem(Source.stem());
        auto Candidate = Destination;
        Candidate += ".uasset";
        for (uint32 Suffix = 1; std::filesystem::exists(Files.ResolvePath(Candidate)) ||
            std::filesystem::exists(Files.ResolvePath(Candidate.parent_path() / Candidate.stem())); ++Suffix)
        {
            Candidate = Destination;
            Candidate += "_" + std::to_string(Suffix) + ".uasset";
        }
        return Candidate;
    }
}

FName ImportStaticMeshObjAsset(const std::filesystem::path& SourcePath,
    UAssetManager& Assets, URenderer& Renderer, FFileManager& Files)
{
    // Importing twice creates another independent asset rather than reusing raw source files.
    const auto Source = Files.ResolvePath(SourcePath);
    const auto Candidate = MakeUniqueAssetDestination(Source, Files);
    if (!FStaticMeshImporter::ImportUStaticMesh(Renderer, Source, Candidate))
        throw std::runtime_error("Static mesh import failed");
    if (!Assets.ScanAssets()) throw std::runtime_error("Imported files were written, but registration failed");
    return UAssetManager::MakeFileAssetName(Candidate, Files);
}

FName ImportStaticMeshAsset(const FStaticMesh& Mesh, const std::filesystem::path& SourcePath,
    bool bFlipTextureV, UAssetManager& Assets, URenderer& Renderer, FFileManager& Files)
{
    const auto Source = Files.ResolvePath(SourcePath);
    const auto Candidate = MakeUniqueAssetDestination(Source, Files);
    if (!FStaticMeshImporter::ImportUStaticMesh(
        Renderer, Mesh, Source.stem(), Candidate, true, bFlipTextureV))
    {
        throw std::runtime_error("Static mesh import failed");
    }
    if (!Assets.ScanAssets()) throw std::runtime_error("Imported files were written, but registration failed");
    return UAssetManager::MakeFileAssetName(Candidate, Files);
}
