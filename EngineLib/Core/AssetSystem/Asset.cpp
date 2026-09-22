#include "Asset.h"

FClassInfo UAsset::ClassInfo("UAsset", UObject::GetClass(), nullptr);

FAssetNameRegistry& UAsset::GetNameRegistry(const FClassInfo* Class)
{
    static TMap<const FClassInfo*, FAssetNameRegistry> Registries;
    return Registries[Class];
}
