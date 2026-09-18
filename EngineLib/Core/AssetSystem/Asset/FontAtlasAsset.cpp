#include "FontAtlasAsset.h"
#include "Core/AssetSystem/AssetSource/FontAtlasAssetSource.h"
#include <cmath>
#include <stdexcept>

IMPLEMENT_CLASS(UFontAtlasAsset, UTexture2DAsset);

void UFontAtlasAsset::Initialize(const FName& Name,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV,
    FFontResource InFontResource, bool InMSDF)
{
    if (InMSDF)
    {
        if (!Texture) throw std::invalid_argument("Missing font atlas texture");
        D3D11_TEXTURE2D_DESC Desc{};
        Texture->GetDesc(&Desc);
        if (InFontResource.GetAtlasWidth() != Desc.Width || InFontResource.GetAtlasHeight() != Desc.Height)
            throw std::invalid_argument("Font atlas image and JSON dimensions differ");
    }
    UTexture2DAsset::Initialize(Name, std::move(Texture), std::move(SRV));
    FontResource = std::move(InFontResource);
    bMSDF = InMSDF;
}

UAsset* FFontAtlasAssetLoader::LoadAsset(const FName& Name, FAssetSource& Source)
{
    auto& FontSource = static_cast<FFontAtlasAssetSource&>(Source);
    try
    {
        const auto& Settings = FontSource.Settings;
        const bool MSDF = FontSource.MetadataSource.has_value();
        if (!MSDF && (Settings.Columns <= 0 || Settings.Columns > 256 ||
            Settings.Rows <= 0 || Settings.Rows > 256 || Settings.Columns * Settings.Rows > 256 ||
            !std::isfinite(Settings.CharacterWidth) || Settings.CharacterWidth <= 0 ||
            !std::isfinite(Settings.CharacterHeight) || Settings.CharacterHeight <= 0 ||
            !std::isfinite(Settings.CharacterAdvance) || Settings.CharacterAdvance < 0))
            throw std::invalid_argument("Invalid bitmap font grid settings");

        FFontResource Font = MSDF ? FFontResource() : FFontResource(Settings.Columns, Settings.Rows,
            Settings.CharacterWidth, Settings.CharacterHeight, Settings.CharacterAdvance);
        if (MSDF && !Font.LoadUnicodeAtlasFromString(FontSource.MetadataSource->ReadFileToString()))
            throw std::runtime_error("Invalid MSDF font atlas JSON");

        // Reuse the DDS/WIC texture loader. This private temporary is never registered
        // with an asset manager; the font asset takes its own COM resource references.
        auto DestroyTemporary = [](UAsset* Asset) { if (Asset) Asset->Destroy(); };
        std::unique_ptr<UAsset, decltype(DestroyTemporary)> Temporary(
            TextureLoader.LoadAsset(Name, FontSource.TextureSource), DestroyTemporary);
        if (!Temporary) return nullptr;
        auto* Texture = Temporary->Cast<UTexture2DAsset>();
        if (!Texture) return nullptr;
        if (MSDF && (Font.GetAtlasWidth() != Texture->GetWidth() || Font.GetAtlasHeight() != Texture->GetHeight()))
            throw std::runtime_error("Font atlas image and JSON dimensions differ");

        auto* Asset = FObjectFactory::ConstructUnInitializedObject<UFontAtlasAsset>();
        if (!Asset) return nullptr;
        try
        {
            Asset->Initialize(Name, Texture->GetTexture(), Texture->GetSRV(), std::move(Font), MSDF);
        }
        catch (...)
        {
            Asset->Destroy();
            throw;
        }
        return Asset;
    }
    catch (const std::exception& Error)
    {
        OutputDebugStringA("Font atlas asset load failed: ");
        OutputDebugStringA(Error.what());
        OutputDebugStringA("\n");
        return nullptr;
    }
}
