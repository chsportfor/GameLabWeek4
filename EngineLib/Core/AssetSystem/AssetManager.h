#pragma once
#include "Asset.h"
#include <filesystem>
#include <unordered_set>

struct FAssetMetaInfo
{
    FName AssetName;
    EAssetType AssetType;
    TSharedPtr<FAssetLoader> AssetLoader;
    TSharedPtr<FAssetSource> AssetSource;
};

// Owns loaded UObjects until Clear()/destruction. Consumers hold non-owning pointers.
// Clear is a shutdown operation: destroy worlds/render collectors before the manager.
class UAssetManager : public UObject
{
    DECLARE_OBJECT(UAssetManager, UObject)
public:
    ~UAssetManager() override { Clear(); }
    UAssetManager() = default;
    UAssetManager(const UAssetManager&) = delete;
    UAssetManager& operator=(const UAssetManager&) = delete;

    static FName NormalizeAssetName(const FName& Name);
    static FName MakeFileAssetName(const std::filesystem::path& Path, const class FFileManager& Files);
    static FName MakeSubAssetName(const FName& FileName, const FString& ItemName);
    void RegisterAsset(const FName& Name, const TSharedPtr<FAssetLoader>& Loader,
        const TSharedPtr<FAssetSource>& Source);
    void RegisterAsset(UAsset* Asset); // Transfers ownership on success.
    UAsset* LoadAsset(const FName& Name);
    UAsset* GetAsset(const FName& Name, bool LoadIfNotLoaded = false);

    template<typename TAsset>
    TAsset* GetAssetAs(const FName& Name, bool LoadIfNotLoaded = false)
    {
        const auto* Meta = AssetMetaInfoMap.Find(NormalizeAssetName(Name));
        if (!Meta || Meta->AssetType != TAssetType<TAsset>::Value) return nullptr;
        auto* Asset = GetAsset(Name, LoadIfNotLoaded);
        return Asset && Asset->GetAssetType() == TAssetType<TAsset>::Value
            ? static_cast<TAsset*>(Asset) : nullptr;
    }
    // Loaded assets cannot be unregistered while raw references may exist.
    bool UnregisterAsset(const FName& Name);
    template<typename Func> void ForEachMetaInfo(Func&& Visitor) const
    {
        for (const auto& [Name, Meta] : AssetMetaInfoMap) Visitor(Meta);
    }
    void Clear();
private:
    TMap<FName, FAssetMetaInfo> AssetMetaInfoMap;
    TMap<FName, UAsset*> LoadedAssets;
    std::unordered_set<FName> LoadingAssets;
};
