#pragma once

#include "Core/AssetSystem/Asset.h"
#include <d3d11.h>
#include <wrl/client.h>

class UTexture2D : public UAsset
{
    DECLARE_OBJECT(UTexture2D, UAsset)
    DECLARE_ASSET_TYPE(UTexture2D)

public:
    void Initialize(
        Microsoft::WRL::ComPtr<ID3D11Texture2D> InTexture,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> InSRV);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> GetTexture() const { return Texture; }
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetSRV() const { return SRV; }
    uint32 GetWidth() const { return Width; }
    uint32 GetHeight() const { return Height; }
    uint32 GetMipLevels() const { return MipLevels; }
    DXGI_FORMAT GetFormat() const { return Format; }

protected:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
    uint32 Width = 0;
    uint32 Height = 0;
    uint32 MipLevels = 0;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
};

class FTexture2DAssetLoader : public FAssetLoader
{
	DECLARE_ASSET_LOADER_TYPE(UTexture2D)
public:
    // Pass Renderer.GetDevice(); loading does not need an immediate context.
    explicit FTexture2DAssetLoader(ID3D11Device* InDevice) : Device(InDevice) {}
    // Register this loader with an FFileAssetSource.
	UAsset* LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;


private:
    Microsoft::WRL::ComPtr<ID3D11Device> Device;
};
