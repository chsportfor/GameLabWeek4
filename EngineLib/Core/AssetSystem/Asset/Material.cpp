#include "Material.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Engine/Assets/ObjImporter.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(UMaterial, UAsset);
IMPLEMENT_SERIALIZATION(UMaterial, UAsset, {});

std::span<const FPropertyInfo> UMaterial::GetDeclaredProperties()
{
    static const FPropertyInfo Properties[] = {
        REFLECT_PROPERTY(UMaterial, DiffuseColor), REFLECT_PROPERTY(UMaterial, DiffuseTexture)
    };
    return Properties;
}

UMaterial* GetDefaultMaterial(UAssetManager& Assets)
{
    const FName name("/Engine/Materials/Default");
    if (auto* material = Assets.GetAssetAs<UMaterial>(name)) return material;
    std::unique_ptr<UMaterial> material(FObjectFactory::ConstructObjectWithName<UMaterial>(name));
    Assets.RegisterAsset(material.get());
    return material.release();
}

void RegisterMaterialLibrary(const std::filesystem::path& Path, UAssetManager& Assets,
    ID3D11Device* Device, FFileManager& Files)
{
    TArray<FObjMaterial> materials;
    FString error;
    if (!FObjImporter::LoadMaterialsFromFile(Path, Files, materials, error))
        throw std::runtime_error(error.CStr());
    const auto fileName = UAssetManager::MakeFileAssetName(Path, Files);
    auto loader = MakeShared<FMaterialAssetLoader>(Device, Assets);
    for (const auto& material : materials)
    {
        Assets.RegisterAsset(UAssetManager::MakeSubAssetName(fileName, material.Name), loader,
            MakeShared<FMaterialAssetSource>(Files, Path, material.Name));
        if (material.DiffuseTexturePath.Len())
        {
            const std::filesystem::path texturePath(material.DiffuseTexturePath.CStr());
            Assets.RegisterAsset(UAssetManager::MakeFileAssetName(texturePath, Files),
                MakeShared<FTexture2DAssetLoader>(Device), MakeShared<FFileAssetSource>(Files, texturePath));
        }
    }
}

UAsset* FMaterialAssetLoader::LoadAsset(const FName& Name, FAssetSource& Source)
{
    const auto& source = static_cast<const FMaterialAssetSource&>(Source);
    TArray<FObjMaterial> materials;
    FString error;
    if (!FObjImporter::LoadMaterialsFromFile(source.FilePath, source.FileManager, materials, error))
        throw std::runtime_error(error.CStr());
    for (const auto& parsed : materials)
    {
        if (!parsed.Name.Equals(source.MaterialName)) continue;
        std::unique_ptr<UMaterial> material(FObjectFactory::ConstructObjectWithName<UMaterial>(Name));
        material->DiffuseColor = {parsed.DiffuseColor.x, parsed.DiffuseColor.y, parsed.DiffuseColor.z, parsed.DiffuseColor.w};
        if (parsed.DiffuseTexturePath.Len())
        {
            const std::filesystem::path texturePath(parsed.DiffuseTexturePath.CStr());
            const auto textureName = UAssetManager::MakeFileAssetName(texturePath, source.FileManager);
            Assets.RegisterAsset(textureName, MakeShared<FTexture2DAssetLoader>(TextureLoader),
                MakeShared<FFileAssetSource>(source.FileManager, texturePath));
            material->DiffuseTexture = Assets.GetAssetAs<UTexture2D>(textureName, true);
            if (!material->DiffuseTexture) return nullptr;
        }
        return material.release();
    }
    throw std::runtime_error("Material not found in MTL file");
}
