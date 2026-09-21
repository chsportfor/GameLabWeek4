#pragma once
#include "Texture2DAsset.h"
#include "Core/Math/Color.h"
#include "Core/AssetSystem/AssetSource/FileAssetSource.h"

class UAssetManager;

class UMaterial : public UAsset
{
    DECLARE_OBJECT(UMaterial, UAsset)
    DECLARE_ASSET_TYPE(UMaterial)
    DECLARE_SERIALIZATION()
public:
    static std::span<const FPropertyInfo> GetDeclaredProperties();
    FLinearColor DiffuseColor{1, 1, 1, 1};
    UTexture2D* DiffuseTexture = nullptr;
};

class FMaterialAssetSource : public FFileAssetSource
{
public:
    FMaterialAssetSource(FFileManager& Files, const std::filesystem::path& Path, const FString& Name)
        : FFileAssetSource(Files, Path), MaterialName(Name) {}
    FString MaterialName;
};

class FMaterialAssetLoader : public FAssetLoader
{
    DECLARE_ASSET_LOADER_TYPE(UMaterial)
public:
    FMaterialAssetLoader(ID3D11Device* Device, UAssetManager& Manager) : TextureLoader(Device), Assets(Manager) {}
    UAsset* LoadAsset(const FName& Name, FAssetSource& Source) override;
private:
    FTexture2DAssetLoader TextureLoader;
    UAssetManager& Assets;
};

UMaterial* GetDefaultMaterial(UAssetManager& Assets);
void RegisterMaterialLibrary(const std::filesystem::path& Path, UAssetManager& Assets,
    ID3D11Device* Device, FFileManager& Files);
