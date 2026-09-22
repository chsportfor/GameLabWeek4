#pragma once

#include "Core/Container/TArray.h"
#include <iosfwd>
#include <filesystem>
#include <span>

/* JSON uasset container, version 1 (one physical file; not Unreal's package format).
 * Exact byte layout, no padding or NUL terminators:
 *   [0..3]   char[4] "UAJS"
 *   [4..7]   uint32 ContainerVersion = 1, little endian
 *   [8..15]  uint64 H = header JSON UTF-8 byte length, little endian
 *   [16..23] uint64 B = body JSON UTF-8 byte length, little endian
 *   [24..24+H)       common header JSON
 *   [24+H..24+H+B)   class-specific body JSON (see each *AssetFile.h)
 *   [24+H+B..EOF)    binary payload; Offset fields are relative to its start
 * 
 * Common header [24..24+H) Example (all keys required):
 *   { "AssetType": "UMaterial", "SchemaVersion": 1,
 *     "Standalone": true, "Dependencies": ["Textures/Body.uasset"] }
 * 
 * AssetType is the registered concrete UObject class name. SchemaVersion is class-specific.
 * Current implementation caps a complete file and each length at INT32_MAX
 * (TArray/FString limit). Lengths describe encoded bytes, not character counts.
 */
struct FFile_uasset
{
    FString AssetType;
    uint32 SchemaVersion = 1;
    bool bStandalone = false;
    TArray<FString> Dependencies;
};

namespace AssetFile
{
    TArray<uint8> SerializeHeader(const FFile_uasset& Header, uint64 BodyByteLength = 0);

    // On success positioned at body JSON; optional output receives its byte length.
    FFile_uasset ReadHeader(std::istream& Stream, uint64* BodyByteLength = nullptr);

    FFile_uasset ReadHeader(std::span<const uint8>& Bytes, uint64* BodyByteLength = nullptr);

    // Replace only the header; keep body JSON and binary payload byte-for-byte.
    // A failed write leaves the original file intact. Does not load an asset object.
    void SetStandalone(const std::filesystem::path& Path, bool Standalone);
}
