#include "MaterialAssetFile.h"
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace
{
    void Validate(const FMaterial_uasset& File)
    {
        if (std::string_view(File.AssetType.CStr()) != "UMaterial")
            throw std::runtime_error("Expected UMaterial asset");
        for (float Value : {File.DiffuseColor.R, File.DiffuseColor.G, File.DiffuseColor.B, File.DiffuseColor.A})
            if (!std::isfinite(Value)) throw std::runtime_error("Material color must be finite");
        if (File.DiffuseTexturePath.Len())
        {
            const std::string Text(File.DiffuseTexturePath.CStr(), File.DiffuseTexturePath.Len());
            const auto Path = std::filesystem::u8path(Text);
            if (Text.find('\0') != std::string::npos || Path.has_root_path() || Path.extension() != ".uasset")
                throw std::runtime_error("Material texture reference must be an asset-root-relative .uasset path");
            for (const auto& Part : Path)
                if (Part == "..") throw std::runtime_error("Material texture reference cannot traverse parent folders");
        }
    }

    void WriteUInt32(TArray<uint8>& Bytes, uint32 Value)
    {
        for (unsigned I = 0; I < 4; ++I) Bytes.Add(uint8(Value >> (8 * I)));
    }

    uint32 ReadUInt32(std::span<const uint8>& Bytes)
    {
        if (Bytes.size() < 4) throw std::runtime_error("Truncated material body");
        const uint32 Value = uint32(Bytes[0]) | (uint32(Bytes[1]) << 8) |
            (uint32(Bytes[2]) << 16) | (uint32(Bytes[3]) << 24);
        Bytes = Bytes.subspan(4);
        return Value;
    }
}

TArray<uint8> AssetFile::Serialize(const FMaterial_uasset& File)
{
    Validate(File);
    FFile_uasset Header = File;
    Header.Dependencies.Empty();
    if (File.DiffuseTexturePath.Len()) Header.Dependencies.Add(File.DiffuseTexturePath);
    auto Bytes = SerializeHeader(Header);
    const uint32 Length = File.DiffuseTexturePath.Len();
    if (uint64(Bytes.Num()) + 20 + Length > uint64((std::numeric_limits<int32>::max)()))
        throw std::runtime_error("Material file exceeds supported size");
    for (float Value : {File.DiffuseColor.R, File.DiffuseColor.G, File.DiffuseColor.B, File.DiffuseColor.A})
        WriteUInt32(Bytes, std::bit_cast<uint32>(Value));
    WriteUInt32(Bytes, Length);
    const auto Offset = Bytes.Num();
    Bytes.SetNum(Offset + static_cast<int32>(Length));
    if (Length) std::memcpy(Bytes.GetData() + Offset, File.DiffuseTexturePath.CStr(), Length);
    return Bytes;
}

FMaterial_uasset AssetFile::DeserializeMaterial(std::span<const uint8> Bytes)
{
    FMaterial_uasset File;
    static_cast<FFile_uasset&>(File) = ReadHeader(Bytes);
    if (std::string_view(File.AssetType.CStr()) != "UMaterial")
        throw std::runtime_error("Asset is not UMaterial");
    File.DiffuseColor.R = std::bit_cast<float>(ReadUInt32(Bytes));
    File.DiffuseColor.G = std::bit_cast<float>(ReadUInt32(Bytes));
    File.DiffuseColor.B = std::bit_cast<float>(ReadUInt32(Bytes));
    File.DiffuseColor.A = std::bit_cast<float>(ReadUInt32(Bytes));
    const uint32 Length = ReadUInt32(Bytes);
    if (Length != Bytes.size()) throw std::runtime_error("Invalid material texture path length or trailing data");
    File.DiffuseTexturePath = FString(std::string_view(reinterpret_cast<const char*>(Bytes.data()), Length));
    Validate(File);
    const int32 ExpectedCount = File.DiffuseTexturePath.Len() ? 1 : 0;
    if (File.Dependencies.Num() != ExpectedCount || (ExpectedCount &&
        std::string_view(File.Dependencies[0].CStr(), File.Dependencies[0].Len()) !=
        std::string_view(File.DiffuseTexturePath.CStr(), File.DiffuseTexturePath.Len())))
        throw std::runtime_error("Material dependency header disagrees with body references");
    return File;
}
