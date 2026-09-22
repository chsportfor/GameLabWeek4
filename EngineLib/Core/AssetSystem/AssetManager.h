#pragma once
#include "Asset.h"
#include <unordered_set>

struct FAssetMetaInfo
{
    FName AssetName; // Asset-root-relative .uasset path; also the object's name.
    const FClassInfo* AssetClass = nullptr;
    bool bStandalone = false;
    TArray<FName> Dependencies; // Unique file references, not material-slot order.
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
    void Initialize(URenderer& InRenderer);

    static FName NormalizeAssetName(const FName& Name);
    static FName MakeFileAssetName(const std::filesystem::path& Path, const class FFileManager& Files);
    bool RegisterAsset(const std::filesystem::path& Path);
    bool ScanAssets(); // Old files are upgraded before registration.
    const FAssetMetaInfo* FindMetaInfo(const FName& Name) const;
    UAsset* LoadAsset(const FName& Name);
    UAsset* GetAsset(const FName& Name, bool LoadIfNotLoaded = false);
    template<typename TAsset> TAsset* GetAssetAs(const FName& Name, bool LoadIfNotLoaded = false)
    {
        const auto Key = NormalizeAssetName(Name);
        if (auto* Asset = GetAsset(Key))
            return Asset->GetRuntimeClass() == TAsset::GetClass() ? static_cast<TAsset*>(Asset) : nullptr;
        // A cache-only query must not load either the requested asset or its default.
        if (!LoadIfNotLoaded || !Key.IsValid()) return nullptr;
        const auto* Meta = FindMetaInfo(Key);
        if (Meta && Meta->AssetClass != TAsset::GetClass()) return nullptr;
        if (auto* Asset = LoadAsset(Key)) return static_cast<TAsset*>(Asset);
        return static_cast<TAsset*>(LoadDefaultAsset(Key, TAsset::GetClass(), TAsset::GetDefaultAssetName()));
    }
    template<typename Func> void ForEachMetaInfo(Func&& Visitor) const
    {
        for (const auto& [Name, Meta] : AssetMetaInfoMap) Visitor(Meta);
    }
    TArray<FName> GetReferencers(const FName& Name) const;
    // Deletes files, not live objects. Uses the registered dependency index.
    // Direct target: no referencers. Cascaded target: also must not be Standalone.
    bool DeleteAsset(const FName& Name);
    void Clear();
private:
    FAssetMetaInfo ReadMetaInfo(const std::filesystem::path& Path, bool UpgradeFile = false) const;
    void RebuildReverseReferences();
    void RetireLoadedAsset(const FName& Name);
    UAsset* LoadDefaultAsset(const FName& MissingName, const FClassInfo* ExpectedClass, const FName& DefaultName);
    bool DeleteUnreferenced(const FName& Name, bool DirectTarget);
    URenderer* Renderer = nullptr;
    std::filesystem::path AssetRoot;
    TMap<FName, FAssetMetaInfo> AssetMetaInfoMap;
    TMap<FName, std::unordered_set<FName>> ReverseReferences;
    TMap<FName, UAsset*> LoadedAssets;
    // Removed file identities stay alive for existing users, but are no longer name-addressable.
    TArray<UAsset*> RetiredAssets;
    std::unordered_set<FName> LoadingAssets;
};
