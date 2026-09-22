#pragma once

#include "Core/Container/TArray.h"
#include <iosfwd>
#include <span>

// In-memory descriptions. Serialize fields explicitly; never write sizeof(struct).
struct FFile_uasset
{
    FString AssetType;
    bool bStandalone = false;
    TArray<FString> Dependencies;
};

namespace AssetFile
{
    // Shared header writer; body serialization remains explicit per asset type.
    TArray<uint8> SerializeHeader(const FFile_uasset& Header);
    // Read from the current position through the last dependency, without reading the body.
    // On success the stream is positioned at the asset-specific body (texture: uint64 DDS size).
    // Throws on read failure, malformed header or unsupported version; no UObject/GPU creation.
    FFile_uasset ReadHeader(std::istream& Stream);

    // Same parser for an already loaded buffer. Advances Bytes past the header on success.
    FFile_uasset ReadHeader(std::span<const uint8>& Bytes);
}
