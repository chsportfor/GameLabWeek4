#include "Asset.h"

const FName& FAsset::GetName() const
{
    return AssetName;
}

FAsset::FAsset(const FName& Name) : AssetName(Name)
{
}
