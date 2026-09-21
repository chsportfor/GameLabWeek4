#include "FontAtlasAsset.h"
#include "Core/AssetSystem/AssetSource/FontAtlasAssetSource.h"
#include <cmath>
#include <stdexcept>


IMPLEMENT_CLASS(UFontAtlasAsset, UTexture2D);

void UFontAtlasAsset::Initialize(
    Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV,
    FFontResource InFontResource, bool InMSDF)
{
    UTexture2D::Initialize(Texture, SRV);
    FontResource = std::move(InFontResource);
    bMSDF = InMSDF;
    if (InMSDF)
    {
        if (!Texture) throw std::invalid_argument("Missing font atlas texture");
        D3D11_TEXTURE2D_DESC Desc{};
        Texture->GetDesc(&Desc);
        if (FontResource.GetAtlasWidth() != Desc.Width || FontResource.GetAtlasHeight() != Desc.Height)
            throw std::invalid_argument("Font atlas image and JSON dimensions differ");
    }
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

        // The temporary texture releases automatically; the atlas retains the COM resources.
        std::unique_ptr<UAsset> Temporary(TextureLoader.LoadAsset(Name, FontSource.TextureSource));
        if (!Temporary) return nullptr;
        auto* Texture = static_cast<UTexture2D*>(Temporary.get());
        if (MSDF && (Font.GetAtlasWidth() != Texture->GetWidth() || Font.GetAtlasHeight() != Texture->GetHeight()))
            throw std::runtime_error("Font atlas image and JSON dimensions differ");

        std::unique_ptr<UFontAtlasAsset> Asset(FObjectFactory::ConstructUnInitializedObject<UFontAtlasAsset>(Name));
        Asset->Initialize(Texture->GetTexture(), Texture->GetSRV(), std::move(Font), MSDF);
        return Asset.release();
    }
    catch (const std::exception& Error)
    {
        OutputDebugStringA("Font atlas asset load failed: ");
        OutputDebugStringA(Error.what());
        OutputDebugStringA("\n");
        return nullptr;
    }
}
