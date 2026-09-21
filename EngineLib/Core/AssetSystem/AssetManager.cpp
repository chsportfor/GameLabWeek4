#include "AssetManager.h"
#include "Core/IO/FileManager.h"
#include <algorithm>
#include <stdexcept>

IMPLEMENT_CLASS(UAssetManager, UObject);

FName UAssetManager::NormalizeAssetName(const FName& Name)
{
    if (!Name.IsValid()) return {};
    std::string text = Name.ToString().CStr();
    std::replace(text.begin(), text.end(), '\\', '/');
    const auto separator = text.find('#');
    const auto suffix = separator == std::string::npos ? std::string{} : text.substr(separator);
    const auto path = std::filesystem::path(text.substr(0, separator)).lexically_normal().generic_string();
    if (path.empty() || path == ".") throw std::invalid_argument("Empty asset path");
    return FName(std::string_view(path + suffix));
}

FName UAssetManager::MakeFileAssetName(const std::filesystem::path& Path, const FFileManager& Files)
{
    const auto absolutePath = Files.ResolvePath(Path);
    const auto relative = absolutePath.lexically_relative(Files.GetFileDirectoryPath());
    // External files used by the OBJ viewer keep an absolute path.
    const auto name = relative.empty() || *relative.begin() == ".." ? absolutePath : relative;
    return NormalizeAssetName(FName(std::string_view(name.generic_string())));
}

FName UAssetManager::MakeSubAssetName(const FName& FileName, const FString& ItemName)
{
    if (ItemName.Len() == 0) throw std::invalid_argument("Empty asset subobject name");
    return NormalizeAssetName(FName(std::string_view(std::string(FileName.ToString().CStr()) + "#" + ItemName.CStr())));
}

void UAssetManager::RegisterAsset(const FName& Name, const TSharedPtr<FAssetLoader>& Loader,
    const TSharedPtr<FAssetSource>& Source)
{
    if (!Loader || !Source) throw std::invalid_argument("Asset registration requires a loader and source");
    const auto key = NormalizeAssetName(Name);
    if (!key.IsValid()) throw std::invalid_argument("Asset registration requires a name");
    const auto type = Loader->GetAssetType();
    if (const auto* existing = AssetMetaInfoMap.Find(key))
    {
        if (existing->AssetType != type) throw std::invalid_argument("Asset path already has another type");
        return;
    }
    AssetMetaInfoMap.Add(key, {key, type, Loader, Source});
    UAsset::GetNameRegistry(type).Add(key);
}

void UAssetManager::RegisterAsset(UAsset* Asset)
{
    if (!Asset) throw std::invalid_argument("Cannot register a null asset");
    const auto key = NormalizeAssetName(Asset->GetName());
    if (!key.IsValid() || AssetMetaInfoMap.Contains(key))
        throw std::invalid_argument("Invalid or already registered asset path");
    Asset->SetName(key);
    AssetMetaInfoMap.Add(key, {key, Asset->GetAssetType(), nullptr, nullptr});
    LoadedAssets.Add(key, Asset);
    UAsset::GetNameRegistry(Asset->GetAssetType()).Add(key);
}

UAsset* UAssetManager::LoadAsset(const FName& Name)
{
    const auto key = NormalizeAssetName(Name);
    if (const auto* loaded = LoadedAssets.Find(key)) return *loaded;
    const auto* entry = AssetMetaInfoMap.Find(key);
    if (!entry || !entry->AssetLoader || !entry->AssetSource) return nullptr;
    // Loading dependencies may register more assets and rehash the metadata map.
    const FAssetMetaInfo meta = *entry;
    if (!LoadingAssets.insert(key).second) throw std::runtime_error("Cyclic asset dependency");
    try
    {
        std::unique_ptr<UAsset> asset(meta.AssetLoader->LoadAsset(key, *meta.AssetSource));
        if (asset)
        {
            if (asset->GetAssetType() != meta.AssetType || asset->GetName() != key)
                throw std::runtime_error("Loader returned the wrong asset identity or type");
            LoadedAssets.Add(key, asset.get());
        }
        LoadingAssets.erase(key);
        return asset.release();
    }
    catch (...) { LoadingAssets.erase(key); throw; }
}

UAsset* UAssetManager::GetAsset(const FName& Name, bool LoadIfNotLoaded)
{
    const auto key = NormalizeAssetName(Name);
    if (const auto* loaded = LoadedAssets.Find(key)) return *loaded;
    return LoadIfNotLoaded ? LoadAsset(key) : nullptr;
}

bool UAssetManager::UnregisterAsset(const FName& Name)
{
    const auto key = NormalizeAssetName(Name);
    if (LoadedAssets.Contains(key) || LoadingAssets.contains(key)) return false;
    if (const auto* meta = AssetMetaInfoMap.Find(key)) UAsset::GetNameRegistry(meta->AssetType).Remove(key);
    AssetMetaInfoMap.Remove(key);
    return true;
}

void UAssetManager::Clear()
{
    for (const auto& [name, asset] : LoadedAssets) delete asset;
    LoadedAssets.Empty();
    for (const auto& [name, meta] : AssetMetaInfoMap) UAsset::GetNameRegistry(meta.AssetType).Remove(name);
    AssetMetaInfoMap.Empty();
    LoadingAssets.clear();
}

UObject* LoadAssetReference(const FName& Name, const FClassInfo* ExpectedClass)
{
    auto* manager = FObjectFactory::GetDefaultAssetManager();
    if (!manager) throw std::runtime_error("Asset reference requires an asset manager");
    auto* asset = manager->GetAsset(Name, true);
    if (!asset || !asset->IsA(ExpectedClass))
        throw std::runtime_error(std::string("Missing or incompatible asset: ") + Name.ToString().CStr());
    return asset;
}
