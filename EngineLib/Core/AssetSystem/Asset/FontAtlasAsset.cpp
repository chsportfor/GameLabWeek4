#include "FontAtlasAsset.h"
#include "Core/IO/FileManager.h"
#include "Rendering/BuiltinAssetNames.h"
#include "Core/AssetSystem/AssetFile/FontAtlasAssetFile.h"
#include "Rendering/Renderer.h"
#include "Editor/Console.h"
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

FName UFontAtlasAsset::GetDefaultAssetName() { return FName(BuiltinAssetNames::DefaultFont); }

void UFontAtlasAsset::Load(const std::filesystem::path& Path, UAssetManager&, URenderer& Renderer)
{
    const auto Bytes = FFileManager::Get().ReadFileToString(Path);
    const auto File = AssetFile::DeserializeFontAtlas({reinterpret_cast<const uint8*>(Bytes.CStr()), size_t(Bytes.Len())});
    auto Texture = Renderer.CreateTexture2DFromMemory(File.Data.GetData(), File.Data.Num());
    if (!Texture) throw std::runtime_error("Font atlas GPU upload failed");
    auto View = Renderer.CreateShaderResourceView(Texture);
    Initialize(std::move(Texture), std::move(View), File.MakeFontResource(), File.bMSDF);
}
