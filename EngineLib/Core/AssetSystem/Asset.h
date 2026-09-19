#pragma once

#include "Core/Core.h"
#include "Core/Name.h"
// IMPORTANT
// Add each concrete asset class here and use DECLARE_ASSET_TYPE in its declaration.
#define ASSET_TYPE_LIST(X) \
    X(FStaticMeshAsset)    \
    X(FTexture2DAsset)     \
    X(FFontAtlasAsset)

enum class EAssetType
{
#define ASSET_ENUM_ENTRY(Class) Class,
    ASSET_TYPE_LIST(ASSET_ENUM_ENTRY)
#undef ASSET_ENUM_ENTRY
};

template<typename T>
struct TAssetType;

#define ASSET_TYPE_ENTRY(Class) \
    class Class; \
    template<> struct TAssetType<Class> \
    { \
        static constexpr EAssetType Value = EAssetType::Class; \
    };

ASSET_TYPE_LIST(ASSET_TYPE_ENTRY)
#undef ASSET_TYPE_ENTRY
#undef ASSET_TYPE_LIST

#define DECLARE_ASSET_TYPE(Class) \
public: \
    EAssetType GetAssetType() const override \
    { \
        return TAssetType<Class>::Value; \
    }

class FAsset
{
public:
    explicit FAsset(const FName& Name);
	virtual ~FAsset() = default;
    const FName& GetName() const;
    virtual EAssetType GetAssetType() const = 0;
private:
    FName AssetName;
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
	virtual TSharedPtr<FAsset> LoadAsset(const FName& AssetName, FAssetSource& AssetSource) = 0;

};
