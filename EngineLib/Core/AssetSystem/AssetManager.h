#pragma once

#include "Asset.h"
#include "Core/Container/TMap.h"
#include <stdexcept>

// Owns loaded assets; render submissions/components can retain shared references.
// A name identifies one source for the lifetime of this manager.
class FAssetManager
{
public:
    template<typename TAsset>
    TSharedPtr<TAsset> Load(const FName& Name, FAssetLoader& Loader, FAssetSource& Source)
    {
        if (const auto* Existing = Assets.Find(Name))
        {
            if (!(*Existing)->template Cast<TAsset>())
                throw std::invalid_argument("Asset name already belongs to another type");
            return std::static_pointer_cast<TAsset>(*Existing);
        }
        TSharedPtr<UAsset> Asset(Loader.LoadAsset(Name, Source));
        if (!Asset || !Asset->template Cast<TAsset>())
            throw std::runtime_error("Failed to load asset: " + std::string(Name.ToString().CStr()));
        Assets.Add(Name, Asset);
        return std::static_pointer_cast<TAsset>(Asset);
    }

    void Clear() { Assets.Empty(); }

private:
    TMap<FName, TSharedPtr<UAsset>> Assets;
};
