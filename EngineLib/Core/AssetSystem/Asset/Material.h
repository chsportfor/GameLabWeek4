#pragma once
#include "Texture2DAsset.h"
#include "Core/Math/Color.h"

class UAssetManager;

class UMaterial : public UAsset
{
    DECLARE_OBJECT(UMaterial, UAsset)
    DECLARE_ASSET_TYPE(UMaterial)
    DECLARE_SERIALIZATION()
public:
    void Load(const std::filesystem::path& Path, UAssetManager& Assets, URenderer& Renderer) override;
    static FName GetDefaultAssetName();
    static std::span<const FPropertyInfo> GetDeclaredProperties();
    FLinearColor DiffuseColor{1, 1, 1, 1};
    UTexture2D* DiffuseTexture = nullptr;
};
