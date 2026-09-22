#include "Texture2DAsset.h"
#include "Core/IO/FileManager.h"
#include "Rendering/BuiltinAssetNames.h"
#include "Rendering/Renderer.h"
#include "Core/AssetSystem/AssetFile/Texture2DAssetFile.h"
#include <cstring>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

IMPLEMENT_CLASS(UTexture2D, UAsset);

void UTexture2D::Initialize(
    ComPtr<ID3D11Texture2D> InTexture, ComPtr<ID3D11ShaderResourceView> InSRV)
{
    if (!InTexture || !InSRV) throw std::invalid_argument("Texture and SRV must both be valid");
    D3D11_TEXTURE2D_DESC Desc{};
    InTexture->GetDesc(&Desc);
    D3D11_SHADER_RESOURCE_VIEW_DESC ViewDesc{};
    InSRV->GetDesc(&ViewDesc);
    ComPtr<ID3D11Resource> ViewResource;
    InSRV->GetResource(&ViewResource);
    ComPtr<ID3D11Texture2D> ViewTexture;
    if (FAILED(ViewResource.As(&ViewTexture)) || ViewTexture.Get() != InTexture.Get() ||
        Desc.ArraySize != 1 || Desc.SampleDesc.Count != 1 || ViewDesc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D)
        throw std::invalid_argument("Expected a matching single 2D texture SRV");

    Texture = std::move(InTexture);
    SRV = std::move(InSRV);
    Width = Desc.Width;
    Height = Desc.Height;
    MipLevels = Desc.MipLevels;
    Format = Desc.Format;
}

FName UTexture2D::GetDefaultAssetName() { return FName(BuiltinAssetNames::DefaultTexture); }

void UTexture2D::Load(const std::filesystem::path& Path, UAssetManager&, URenderer& Renderer)
{
    const auto Bytes = FFileManager::Get().ReadFileToString(Path);
    const auto File = AssetFile::DeserializeTexture2D({reinterpret_cast<const uint8*>(Bytes.CStr()), size_t(Bytes.Len())});
    auto Texture = Renderer.CreateTexture2DFromMemory(File.Data.GetData(), File.Data.Num());
    if (!Texture) throw std::runtime_error("Texture GPU upload failed");
    auto View = Renderer.CreateShaderResourceView(Texture);
    Initialize(std::move(Texture), std::move(View));
}
