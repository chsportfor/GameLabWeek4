#include "MaterialImporter.h"
#include "Texture2DImporter.h"
#include "Core/AssetSystem/AssetFile/MaterialAssetFile.h"
#include "Core/AssetSystem/AssetFile/Texture2DAssetFile.h"
#include "Core/IO/FileManager.h"
#include "Editor/Console.h"
#include "Engine/Assets/ObjImporter.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    FString RelativeAssetPath(const fs::path& Path)
    {
        const auto Relative = fs::relative(Path, FFileManager::Get().GetFileDirectoryPath());
        if (Relative.empty() || Relative.is_absolute() || *Relative.begin() == ".." || Path.extension() != ".uasset")
            throw std::runtime_error("Material texture must be a .uasset inside the asset root");
        const auto UTF8 = Relative.generic_u8string();
        return FString(std::string_view(reinterpret_cast<const char*>(UTF8.data()), UTF8.size()));
    }

    FString ValidateTextureAsset(const fs::path& Requested)
    {
        const auto Path = FFileManager::Get().ResolvePath(Requested);
        const auto Relative = RelativeAssetPath(Path);
        std::ifstream Stream(Path, std::ios::binary);
        const auto Header = AssetFile::ReadHeader(Stream);
        if (std::string_view(Header.AssetType.CStr()) != "UTexture2D")
            throw std::runtime_error("Material diffuse reference must point to a UTexture2D asset");
        return Relative;
    }

    fs::path MaterialFileName(const FString& Name)
    {
        const std::string Text(Name.CStr(), Name.Len());
        if (Text.empty() || Text == "." || Text == ".." || Text.back() == '.' || Text.back() == ' ' ||
            Text.find_first_of("<>:\"/\\|?*") != std::string::npos ||
            std::any_of(Text.begin(), Text.end(), [](unsigned char C) { return C < 32; }))
            throw std::runtime_error("MTL material name cannot be used as a file name: " + Text);
        return fs::u8path(Text + ".uasset");
    }

}

FMaterialImporter::FPreparedMaterials FMaterialImporter::PrepareMaterials(URenderer& Renderer,
    const TArray<FObjMaterial>& Materials, const fs::path& MaterialDirectory,
    const fs::path& TextureDirectory, bool bStandalone)
{
    auto& Files = FFileManager::Get();
    FPreparedMaterials Prepared;
    std::vector<std::pair<fs::path, FString>> ImportedTextures;
    for (const auto& Parsed : Materials)
    {
        const auto Target = MaterialDirectory / MaterialFileName(Parsed.Name);
        Prepared.MaterialPaths.Add(RelativeAssetPath(Target));
        FMaterial_uasset File;
        File.bStandalone = bStandalone;
        File.DiffuseColor = {Parsed.DiffuseColor.x, Parsed.DiffuseColor.y, Parsed.DiffuseColor.z,
            Parsed.bHasDissolve ? Parsed.Dissolve :
            (Parsed.bHasTransparency ? 1.0f - Parsed.Transparency : Parsed.DiffuseColor.w)};
        if (Parsed.DiffuseTexturePath.Len())
        {
            // The existing MTL parser already resolves this relative to the MTL's directory.
            const auto TextureSource = Files.ResolvePath(fs::u8path(Parsed.DiffuseTexturePath.CStr()));
            if (TextureSource.extension() == ".uasset")
                File.DiffuseTexturePath = ValidateTextureAsset(TextureSource);
            else
            {
                for (const auto& [Existing, Name] : ImportedTextures)
                {
                    if (fs::equivalent(Existing, TextureSource)) { File.DiffuseTexturePath = Name; break; }
                }
                if (!File.DiffuseTexturePath.Len())
                {
                    const auto TextureTarget = TextureDirectory / (TextureSource.stem().wstring() + L".uasset");
                    File.DiffuseTexturePath = RelativeAssetPath(TextureTarget);
                    const auto TextureFile = FTexture2DImporter::PrepareTexture2D(Renderer, TextureSource, false);
                    Prepared.Files.push_back({TextureTarget, AssetFile::Serialize(TextureFile)});
                    ImportedTextures.emplace_back(TextureSource, File.DiffuseTexturePath);
                }
            }
        }
        Prepared.Files.push_back({Target, AssetFile::Serialize(File)});
    }
    return Prepared;
}

bool FMaterialImporter::ImportUMaterial(const fs::path& TextureAssetPath,
    const FLinearColor& DiffuseColor, const fs::path& Destination, bool bStandalone)
{
    try
    {
        FMaterial_uasset File;
        File.bStandalone = bStandalone;
        File.DiffuseColor = DiffuseColor;
        if (!TextureAssetPath.empty()) File.DiffuseTexturePath = ValidateTextureAsset(TextureAssetPath);
        const auto Bytes = AssetFile::Serialize(File);
        return WriteImportedAsset(Destination, {Bytes.GetData(), static_cast<size_t>(Bytes.Num())});
    }
    catch (const std::exception& Error)
    {
        UE_LOG(Error, Core, "Material import failed: %s", Error.what());
        return false;
    }
}

bool FMaterialImporter::ImportUMaterial(URenderer& Renderer, const fs::path& MtlPath,
    const fs::path& DestinationDirectory, bool bStandalone)
{
    try
    {
        auto& Files = FFileManager::Get();
        const auto Source = Files.ResolvePath(MtlPath);
        const auto Directory = Files.ResolvePath(DestinationDirectory.empty()
            ? fs::path("Materials") : DestinationDirectory);
        if (Directory.extension() == ".uasset")
            throw std::runtime_error("MTL import destination must be a directory, not a .uasset file");
        TArray<FObjMaterial> Materials;
        FString Error;
        if (!FObjImporter::LoadMaterialsFromFile(Source, Files, Materials, Error))
            throw std::runtime_error(Error.CStr());
        if (Materials.IsEmpty()) throw std::runtime_error("MTL contains no materials");

        const auto Prepared = PrepareMaterials(Renderer, Materials, Directory, Files.ResolvePath("Textures"), bStandalone);
        // Build views only after preparation is complete, so no reallocation invalidates their data.
        std::vector<FAssetFileToWrite> Outputs;
        Outputs.reserve(Prepared.Files.size());
        for (const auto& File : Prepared.Files)
            Outputs.push_back({File.Path, {File.Bytes.GetData(), static_cast<size_t>(File.Bytes.Num())}});
        return WriteImportedAssets(Outputs);
    }
    catch (const std::exception& Error)
    {
        UE_LOG(Error, Core, "MTL material import failed: %s", Error.what());
        return false;
    }
}
