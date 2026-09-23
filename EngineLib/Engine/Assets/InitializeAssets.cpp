#include "InitializeAssets.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/IO/FileManager.h"
#include "Engine/Assets/Importers/StaticMeshImporter.h"
#include <stdexcept>

namespace
{
    FName RegisterImportedAssets(const TArray<FName>& Names, UAssetManager& Assets)
    {
        if (Names.IsEmpty()) return {};
        bool Registered = true;
        for (const FName& Name : Names)
        {
            if (!Assets.RegisterAsset(std::filesystem::u8path(Name.ToString().CStr())))
                Registered = false;
        }
        if (!Registered) throw std::runtime_error("Imported files were written, but registration failed");
        return Names[0];
    }

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
    const auto Names = FStaticMeshImporter::ImportUStaticMesh(Renderer, Source, Candidate);
    return RegisterImportedAssets(Names, Assets);
}

FName ImportStaticMeshAsset(const FStaticMesh& Mesh, const std::filesystem::path& SourcePath,
    bool bFlipTextureV, UAssetManager& Assets, URenderer& Renderer, FFileManager& Files,
    const std::filesystem::path& Destination)
{
    const auto Source = Files.ResolvePath(SourcePath);
    const auto Candidate = Destination.empty() ? MakeUniqueAssetDestination(Source, Files) : Destination;
    const auto Names = FStaticMeshImporter::ImportUStaticMesh(
        Renderer, Mesh, Source.stem(), Candidate, true, bFlipTextureV);
    return RegisterImportedAssets(Names, Assets);
}
