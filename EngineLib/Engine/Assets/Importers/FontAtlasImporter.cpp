#include "FontAtlasImporter.h"
#include "Texture2DImporter.h"
#include "Core/AssetSystem/AssetFile/FontAtlasAssetFile.h"
#include "Core/AssetSystem/AssetFile/Texture2DAssetFile.h"
#include "Core/IO/FileManager.h"
#include "Rendering/Renderer.h"
#include "Editor/Console.h"
#include <stdexcept>

TArray<FName> FFontAtlasImporter::ImportUFontAtlas(URenderer& Renderer,
    const std::filesystem::path& ImagePath, const std::filesystem::path& MetadataPath,
    const std::filesystem::path& Destination, bool bStandalone)
{
    try
    {
        FFontAtlas_uasset File;
        File.bStandalone = bStandalone;
        File.bMSDF = true;
        File.MetadataJson = FFileManager::Get().ReadFileToString(MetadataPath);
        return Import(Renderer, ImagePath, Destination, std::move(File));
    }
    catch (const std::exception& Error)
    {
        UE_DEBUG_LOG_ERROR(Core, "Font atlas import failed: %s", Error.what());
        return {};
    }
}

TArray<FName> FFontAtlasImporter::ImportUFontAtlas(URenderer& Renderer,
    const std::filesystem::path& ImagePath, const FBitmapFontAtlasSettings& Settings,
    const std::filesystem::path& Destination, bool bStandalone)
{
    FFontAtlas_uasset File;
    File.bStandalone = bStandalone;
    File.BitmapSettings = Settings;
    return Import(Renderer, ImagePath, Destination, std::move(File));
}

TArray<FName> FFontAtlasImporter::Import(URenderer& Renderer, const std::filesystem::path& ImagePath,
    const std::filesystem::path& Destination, FFontAtlas_uasset File)
{
    try
    {
        auto& Files = FFileManager::Get();
        const auto Source = Files.ResolvePath(ImagePath);
        auto Target = Files.ResolvePath(Destination.empty() ? std::filesystem::path("Fonts") : Destination);
        if (Target.extension() != ".uasset") Target /= Source.stem().wstring() + L".uasset";
        if (std::filesystem::exists(Target)) throw std::runtime_error("Font asset destination already exists");

        // Reuse DDS conversion and GPU validation, but never write/register a texture asset.
        auto TextureFile = FTexture2DImporter::PrepareTexture2D(Renderer, Source, false, false);
        File.Data = std::move(TextureFile.Data);
        const auto Font = File.MakeFontResource();
        if (File.bMSDF)
        {
            auto Texture = Renderer.CreateTexture2DFromMemory(File.Data.GetData(), File.Data.Num());
            if (!Texture) throw std::runtime_error("Could not validate font atlas texture");
            D3D11_TEXTURE2D_DESC Desc{};
            Texture->GetDesc(&Desc);
            if (Font.GetAtlasWidth() != Desc.Width || Font.GetAtlasHeight() != Desc.Height)
                throw std::runtime_error("Font atlas image and JSON dimensions differ");
        }
        const auto Bytes = AssetFile::Serialize(File);
        return WriteImportedAsset(Target, {Bytes.GetData(), static_cast<size_t>(Bytes.Num())});
    }
    catch (const std::exception& Error)
    {
        UE_DEBUG_LOG_ERROR(Core, "Font atlas import failed: %s", Error.what());
        return {};
    }
}
