#include "Material.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/IO/FileManager.h"
#include "Rendering/BuiltinAssetNames.h"
#include "Core/AssetSystem/AssetFile/MaterialAssetFile.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(UMaterial, UAsset);
IMPLEMENT_SERIALIZATION(UMaterial, UAsset, {});

std::span<const FPropertyInfo> UMaterial::GetDeclaredProperties()
{
    static const FPropertyInfo Properties[] = {
        REFLECT_PROPERTY(UMaterial, DiffuseColor), REFLECT_PROPERTY(UMaterial, DiffuseTexture)
    };
    return Properties;
}

FName UMaterial::GetDefaultAssetName() { return FName(BuiltinAssetNames::DefaultMaterial); }

void UMaterial::Load(const std::filesystem::path& Path, UAssetManager& Assets, URenderer&)
{
    const auto Bytes = FFileManager::Get().ReadFileToString(Path);
    const auto File = AssetFile::DeserializeMaterial({reinterpret_cast<const uint8*>(Bytes.CStr()), size_t(Bytes.Len())});
    DiffuseColor = File.DiffuseColor;
    if (File.DiffuseTexturePath.Len())
    {
        DiffuseTexture = Assets.GetAssetAs<UTexture2D>(FName(File.DiffuseTexturePath), true);
        if (!DiffuseTexture) throw std::runtime_error("Material texture dependency could not be loaded");
    }
}
