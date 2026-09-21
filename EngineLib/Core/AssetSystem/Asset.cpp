#include "Asset.h"
#include <stdexcept>

FClassInfo UAsset::ClassInfo("UAsset", UObject::GetClass(), nullptr);


FAssetNameRegistry& UAsset::GetNameRegistry(EAssetType Type)
{
    switch (Type)
    {
#define ASSET_NAME_REGISTRY_CASE(Class) \
    case EAssetType::Class: return TAssetType<Class>::GetNameRegistry();
        ASSET_TYPE_LIST(ASSET_NAME_REGISTRY_CASE)
#undef ASSET_NAME_REGISTRY_CASE
    }
    throw std::invalid_argument("Unknown asset type");
}
