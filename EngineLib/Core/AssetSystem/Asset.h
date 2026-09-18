#pragma once

#include "Core/Core.h"
#include "Core/Name.h"
#include "Core/Object/Object.h"

class UAsset : public UObject
{
	DECLARE_OBJECT(UAsset, UObject)
public:
	void Initialize() = delete;
	virtual ~UAsset() = default;
protected:
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

	virtual UAsset* LoadAsset(const FName& AssetName, FAssetSource& AssetSource) = 0;

};
