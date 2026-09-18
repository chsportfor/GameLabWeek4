#pragma once

#include "Texture2DAsset.h"
#include "Rendering/FontResource.h"

class UFontAtlasAsset : public UTexture2DAsset
{
    DECLARE_OBJECT(UFontAtlasAsset, UTexture2DAsset)
public:
    void Initialize(const FName& Name, Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture,
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
    UAsset* LoadAsset(const FName& Name, FAssetSource& Source) override;


private:
    FTexture2DAssetLoader TextureLoader;
};
