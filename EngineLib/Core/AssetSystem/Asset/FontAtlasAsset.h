#pragma once

#include "Texture2DAsset.h"
#include "Rendering/FontResource.h"

class FFontAtlasAsset : public FTexture2DAsset
{
    DECLARE_ASSET_TYPE(FFontAtlasAsset)

public:
    FFontAtlasAsset(const FName& Name, Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV,
        FFontResource InFontResource, bool InMSDF);

    const FCharacterInfo* FindCharacter(char Character) const { return FontResource.FindCharacter(Character); }
    const FCharacterInfo* FindUnicodeCharacter(uint32 CodePoint) const { return FontResource.FindUnicodeCharacter(CodePoint); }
    const FFontResource& GetFontResource() const { return FontResource; }
    bool IsMSDF() const { return bMSDF; }
    float GetDistanceRange() const { return FontResource.GetDistanceRange(); }

private:
    FFontResource FontResource;
    bool bMSDF = false;
};

class FFontAtlasAssetLoader : public FAssetLoader
{
public:
    explicit FFontAtlasAssetLoader(ID3D11Device* Device) : TextureLoader(Device) {}
    // Register with FFontAtlasAssetSource (texture + grid settings or JSON).
	TSharedPtr<FAsset> LoadAsset(const FName& Name, FAssetSource& Source) override;


private:
    FTexture2DAssetLoader TextureLoader;
};
