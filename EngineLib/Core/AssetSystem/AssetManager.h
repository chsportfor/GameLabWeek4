#pragma once

#include "Asset.h"
#include "Core/Container/TMap.h"
#include <stdexcept>

struct FAssetMetaInfo
{
    FName AssetName;
    EAssetType AssetType;
    TSharedPtr<FAssetLoader> AssetLoader;
    TSharedPtr<FAssetSource> AssetSource;
};

// Registration survives unloading. Only LoadedAssets owns the loaded instances.
class FAssetManager
{
public:
    FAssetManager()
    {
        // Construct the shared lists first so they outlive even a static manager.
#define INITIALIZE_ASSET_NAMES(Class) FAsset::GetNameRegistry(EAssetType::Class);
        ASSET_TYPE_LIST(INITIALIZE_ASSET_NAMES)
#undef INITIALIZE_ASSET_NAMES
    }
    ~FAssetManager() { Clear(); }
    FAssetManager(const FAssetManager&) = delete;
    FAssetManager& operator=(const FAssetManager&) = delete;
    FAssetManager(FAssetManager&&) = delete;
    FAssetManager& operator=(FAssetManager&&) = delete;

    void RegisterAsset(const FName& Name, const TSharedPtr<FAssetLoader>& Loader,
        const TSharedPtr<FAssetSource>& Source)
    {
        if (AssetMetaInfoMap.Find(Name)) return;
        if (!Loader || !Source) throw std::invalid_argument("Asset registration requires a loader and source");
        const auto Type = Loader->GetAssetType();
        AssetMetaInfoMap.Add(Name, {Name, Type, Loader, Source});
        FAsset::GetNameRegistry(Type).Add(Name);
    }

    void RegisterAsset(const TSharedPtr<FAsset>& Asset)
    {
        if (!Asset) throw std::invalid_argument("Cannot register a null asset");
        const auto& Name = Asset->GetName();
        if (AssetMetaInfoMap.Find(Name)) return;
        const auto Type = Asset->GetAssetType();
        AssetMetaInfoMap.Add(Name, {Name, Type, nullptr, nullptr});
        FAsset::GetNameRegistry(Type).Add(Name);
        LoadedAssets.Add(Name, Asset);
    }

    TSharedPtr<FAsset> LoadAsset(const FName& Name)
    {
        if (const auto* Loaded = LoadedAssets.Find(Name)) return *Loaded;
        const auto* Meta = AssetMetaInfoMap.Find(Name);
        if (!Meta || !Meta->AssetLoader || !Meta->AssetSource) return nullptr;
        auto Asset = Meta->AssetLoader->LoadAsset(Name, *Meta->AssetSource);
        if (Asset) LoadedAssets.Add(Name, Asset);
        return Asset;
    }

    TSharedPtr<FAsset> GetAsset(const FName& Name, bool LoadIfNotLoaded = false)
    {
        if (const auto* Loaded = LoadedAssets.Find(Name)) return *Loaded;
        return LoadIfNotLoaded ? LoadAsset(Name) : nullptr;
    }

    template<typename TAsset>
    TSharedPtr<TAsset> GetAssetAs(const FName& Name, bool LoadIfNotLoaded = false)
    {
        auto Asset = GetAsset(Name, LoadIfNotLoaded);
        if (!Asset || Asset->GetAssetType() != TAssetType<TAsset>::Value) return nullptr;
        return std::static_pointer_cast<TAsset>(Asset);
    }

    // Release the manager's ownership; active users retain their shared references.
    void UnloadAsset(const FName& Name) { LoadedAssets.Remove(Name); }
    void UnregisterAsset(const FName& Name)
    {
        if (const auto* Meta = AssetMetaInfoMap.Find(Name))
            FAsset::GetNameRegistry(Meta->AssetType).Remove(Name);
        UnloadAsset(Name);
        AssetMetaInfoMap.Remove(Name);
    }
    template<typename Func>
    void ForEachMetaInfo(Func&& Visitor) const
    {
        for (const auto& [Name, Meta] : AssetMetaInfoMap) Visitor(Meta);
    }
    void Clear()
    {
        for (const auto& [Name, Meta] : AssetMetaInfoMap)
            FAsset::GetNameRegistry(Meta.AssetType).Remove(Name);
        AssetMetaInfoMap.Empty();
        LoadedAssets.Empty();
    }

private:
    TMap<FName, FAssetMetaInfo> AssetMetaInfoMap;
    TMap<FName, TSharedPtr<FAsset>> LoadedAssets;
};
