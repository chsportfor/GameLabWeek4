#pragma once

#include "Core/Core.h"
#include "Core/Name.h"
#include "Core/Container/TArray.h"
#include "Core/Container/TMap.h"
#include "Core/Object/Object.h"
// IMPORTANT
// Add each concrete asset class here and use DECLARE_ASSET_TYPE in its declaration.
#define ASSET_TYPE_LIST(X) \
    X(UStaticMeshAsset)    \
    X(UTexture2D)     \
    X(UFontAtlasAsset)    \
    X(UMaterial)

enum class EAssetType
{
#define ASSET_ENUM_ENTRY(Class) Class,
    ASSET_TYPE_LIST(ASSET_ENUM_ENTRY)
#undef ASSET_ENUM_ENTRY
};

// Counts registrations across managers; each name appears only once in Names.
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

template<typename T>
struct TAssetType;

#define ASSET_TYPE_ENTRY(Class) \
    class Class; \
    template<> struct TAssetType<Class> \
    { \
        static constexpr EAssetType Value = EAssetType::Class; \
        static FAssetNameRegistry& GetNameRegistry() \
        { \
            static FAssetNameRegistry Registry; \
            return Registry; \
        } \
    };

ASSET_TYPE_LIST(ASSET_TYPE_ENTRY)
#undef ASSET_TYPE_ENTRY

#define DECLARE_ASSET_LOADER_TYPE(Class) \
public: \
    EAssetType GetAssetType() const override \
    { \
        return TAssetType<Class>::Value; \
    }

#define DECLARE_ASSET_TYPE(Class) \
    DECLARE_ASSET_LOADER_TYPE(Class) \
    static const TArray<FName>& GetRegisteredAssetNames() \
    { \
        return TAssetType<Class>::GetNameRegistry().GetNames(); \
    }

class UAsset : public UObject
{
    DECLARE_OBJECT(UAsset, UObject)
public:
	virtual ~UAsset() = default;
    virtual EAssetType GetAssetType() const = 0;
private:
    friend class UAssetManager;
    static FAssetNameRegistry& GetNameRegistry(EAssetType Type);
};

class FAssetSource
{
public:
	virtual ~FAssetSource() = default;
};

class FAssetLoader
{
public:
	virtual ~FAssetLoader() = default;
	virtual EAssetType GetAssetType() const = 0;
	virtual UAsset* LoadAsset(const FName& AssetName, FAssetSource& AssetSource) = 0;

};
