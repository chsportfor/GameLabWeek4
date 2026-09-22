#pragma once
#include "Core/Core.h"
#include "Core/Name.h"
#include "Core/Container/TArray.h"
#include "Core/Container/TMap.h"
#include "Core/Object/Object.h"
#include <filesystem>

class UAssetManager;
class URenderer;

// Counts registrations across managers; each name appears only once.
class FAssetNameRegistry
{
public:
    const TArray<FName>& GetNames() const { return Names; }
    void Add(const FName& Name)
    {
        if (auto* Count = RegistrationCounts.Find(Name)) { ++*Count; return; }
        RegistrationCounts.Add(Name, 1);
        Names.Add(Name);
    }
    void Remove(const FName& Name)
    {
        auto* Count = RegistrationCounts.Find(Name);
        if (!Count || --*Count != 0) return;
        RegistrationCounts.Remove(Name);
        Names.Remove(Name);
    }
private:
    TArray<FName> Names;
    TMap<FName, uint32> RegistrationCounts;
};

// Exact class registry: font atlases are not listed as texture assets.
#define DECLARE_ASSET_TYPE(Class) \
public: \
    static const TArray<FName>& GetRegisteredAssetNames() \
    { return GetNameRegistry(Class::GetClass()).GetNames(); }

class UAsset : public UObject
{
    DECLARE_OBJECT(UAsset, UObject)
public:
    virtual ~UAsset() = default;
    // Called once after construction and assignment of the asset path.
    // Throw on failure; the manager discards the partially loaded object.
    virtual void Load(const std::filesystem::path& Path, UAssetManager& Assets, URenderer& Renderer) = 0;
    static FName GetDefaultAssetName() { return {}; }
protected:
    static FAssetNameRegistry& GetNameRegistry(const FClassInfo* Class);
private:
    friend class UAssetManager;
};
