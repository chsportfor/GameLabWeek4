#include "StaticMeshAssetFile.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <set>
#include <stdexcept>

namespace
{
    std::array<float, 12> VertexValues(const FVertexSimple& V)
    {
        return {V.x, V.y, V.z, V.nx, V.ny, V.nz, V.r, V.g, V.b, V.a, V.u, V.v};
    }

    TArray<FString> Dependencies(const FStaticMesh_uasset& File)
    {
        TArray<FString> Result;
        std::set<std::string> Seen;
        for (const auto& Name : File.MaterialPaths)
            if (Seen.emplace(Name.CStr(), Name.Len()).second) Result.Add(Name);
        return Result;
    }

    void Validate(const FStaticMesh_uasset& File)
    {
        if (std::string_view(File.AssetType.CStr()) != "UStaticMeshAsset")
            throw std::runtime_error("Expected UStaticMeshAsset");
        const auto& Geometry = File.Geometry;
        if (Geometry.Vertices.IsEmpty() || Geometry.Indices.IsEmpty() || Geometry.Indices.Num() % 3 ||
            File.Sections.IsEmpty() || File.MaterialPaths.IsEmpty())
            throw std::runtime_error("Static mesh requires vertices, triangles, sections and material slots");
        for (const auto& V : Geometry.Vertices)
            for (float Value : VertexValues(V))
                if (!std::isfinite(Value)) throw std::runtime_error("Non-finite mesh vertex");
        FBoundingBox Bounds(Geometry.Vertices[0].GetPosition(), Geometry.Vertices[0].GetPosition());
        for (const auto& V : Geometry.Vertices) Bounds.ExpandToInclude(V.GetPosition());
        if (File.Bounds.Min.x != Bounds.Min.x || File.Bounds.Min.y != Bounds.Min.y || File.Bounds.Min.z != Bounds.Min.z ||
            File.Bounds.Max.x != Bounds.Max.x || File.Bounds.Max.y != Bounds.Max.y || File.Bounds.Max.z != Bounds.Max.z)
            throw std::runtime_error("Mesh bounds disagree with geometry");
        for (uint32 Index : Geometry.Indices)
            if (Index >= uint32(Geometry.Vertices.Num())) throw std::runtime_error("Mesh index out of range");
        uint32 NextIndex = 0;
        for (const auto& Section : File.Sections)
        {
            if (Section.FirstIndex != NextIndex || !Section.IndexCount || Section.IndexCount % 3 ||
                Section.IndexCount > uint32(Geometry.Indices.Num()) - NextIndex ||
                Section.MaterialIndex >= uint32(File.MaterialPaths.Num()))
                throw std::runtime_error("Invalid mesh section range or material slot");
            NextIndex += Section.IndexCount;
        }
        if (NextIndex != uint32(Geometry.Indices.Num())) throw std::runtime_error("Mesh sections must cover all indices");
        for (const auto& Name : File.MaterialPaths)
        {
            const std::string Text(Name.CStr(), Name.Len());
            const auto Path = std::filesystem::u8path(Text);
            if (Text.empty() || Text.find('\0') != std::string::npos || Path.has_root_path() || Path.extension() != ".uasset")
                throw std::runtime_error("Mesh material must be an asset-root-relative .uasset path");
            for (const auto& Part : Path)
                if (Part == "..") throw std::runtime_error("Mesh material path cannot traverse parent folders");
        }
    }

    void WriteUInt32(TArray<uint8>& Bytes, uint32 Value)
    {
        for (unsigned I = 0; I < 4; ++I) Bytes.Add(uint8(Value >> (8 * I)));
    }

    uint32 ReadUInt32(std::span<const uint8>& Bytes)
    {
        if (Bytes.size() < 4) throw std::runtime_error("Truncated static mesh body");
        const uint32 Value = uint32(Bytes[0]) | (uint32(Bytes[1]) << 8) |
            (uint32(Bytes[2]) << 16) | (uint32(Bytes[3]) << 24);
        Bytes = Bytes.subspan(4);
        return Value;
    }

    int32 ReadCount(std::span<const uint8>& Bytes, size_t MinimumElementSize)
    {
        const auto Count = ReadUInt32(Bytes);
        if (Count > uint32((std::numeric_limits<int32>::max)()) || Count > Bytes.size() / MinimumElementSize)
            throw std::runtime_error("Invalid static mesh array length");
        return int32(Count);
    }

    float ReadFloat(std::span<const uint8>& Bytes) { return std::bit_cast<float>(ReadUInt32(Bytes)); }
}

TArray<uint8> AssetFile::Serialize(const FStaticMesh_uasset& File)
{
    Validate(File);
    FFile_uasset Header = File;
    Header.Dependencies = Dependencies(File);
    auto Bytes = SerializeHeader(Header);
    uint64 Size = uint64(Bytes.Num()) + 40 + uint64(File.Geometry.Vertices.Num()) * 48 +
        uint64(File.Geometry.Indices.Num()) * 4 + uint64(File.Sections.Num()) * 12;
    for (const auto& Path : File.MaterialPaths) Size += 4 + uint64(Path.Len());
    if (Size > uint64((std::numeric_limits<int32>::max)())) throw std::runtime_error("Static mesh file too large");
    Bytes.Reserve(uint32(Size));
    WriteUInt32(Bytes, File.Geometry.Vertices.Num());
    for (const auto& V : File.Geometry.Vertices)
        for (float Value : VertexValues(V)) WriteUInt32(Bytes, std::bit_cast<uint32>(Value));
    WriteUInt32(Bytes, File.Geometry.Indices.Num());
    for (uint32 Index : File.Geometry.Indices) WriteUInt32(Bytes, Index);
    for (float Value : {File.Bounds.Min.x, File.Bounds.Min.y, File.Bounds.Min.z,
        File.Bounds.Max.x, File.Bounds.Max.y, File.Bounds.Max.z}) WriteUInt32(Bytes, std::bit_cast<uint32>(Value));
    WriteUInt32(Bytes, File.Sections.Num());
    for (const auto& S : File.Sections)
    {
        WriteUInt32(Bytes, S.FirstIndex);
        WriteUInt32(Bytes, S.IndexCount);
        WriteUInt32(Bytes, S.MaterialIndex);
    }
    WriteUInt32(Bytes, File.MaterialPaths.Num());
    for (const auto& Path : File.MaterialPaths)
    {
        WriteUInt32(Bytes, Path.Len());
        const int32 Offset = Bytes.Num();
        Bytes.SetNum(Offset + Path.Len());
        std::memcpy(Bytes.GetData() + Offset, Path.CStr(), Path.Len());
    }
    return Bytes;
}

FStaticMesh_uasset AssetFile::DeserializeStaticMesh(std::span<const uint8> Bytes)
{
    FStaticMesh_uasset File;
    static_cast<FFile_uasset&>(File) = ReadHeader(Bytes);
    if (std::string_view(File.AssetType.CStr()) != "UStaticMeshAsset") throw std::runtime_error("Asset is not UStaticMeshAsset");
    File.Geometry.Vertices.SetNum(ReadCount(Bytes, 48));
    for (auto& V : File.Geometry.Vertices)
    {
        std::array<float, 12> Values;
        for (auto& Value : Values) Value = ReadFloat(Bytes);
        V = {Values[0], Values[1], Values[2], Values[3], Values[4], Values[5],
            Values[6], Values[7], Values[8], Values[9], Values[10], Values[11]};
    }
    File.Geometry.Indices.SetNum(ReadCount(Bytes, 4));
    for (auto& Index : File.Geometry.Indices) Index = ReadUInt32(Bytes);
    File.Bounds.Min.x = ReadFloat(Bytes); File.Bounds.Min.y = ReadFloat(Bytes); File.Bounds.Min.z = ReadFloat(Bytes);
    File.Bounds.Max.x = ReadFloat(Bytes); File.Bounds.Max.y = ReadFloat(Bytes); File.Bounds.Max.z = ReadFloat(Bytes);
    File.Sections.SetNum(ReadCount(Bytes, 12));
    for (auto& S : File.Sections)
    {
        S.FirstIndex = ReadUInt32(Bytes); S.IndexCount = ReadUInt32(Bytes); S.MaterialIndex = ReadUInt32(Bytes);
    }
    File.MaterialPaths.SetNum(ReadCount(Bytes, 4));
    for (auto& Path : File.MaterialPaths)
    {
        const auto Length = ReadCount(Bytes, 1);
        Path = FString(std::string_view(reinterpret_cast<const char*>(Bytes.data()), Length));
        Bytes = Bytes.subspan(Length);
    }
    if (!Bytes.empty()) throw std::runtime_error("Trailing static mesh data");
    Validate(File);
    const auto Expected = Dependencies(File);
    if (Expected.Num() != File.Dependencies.Num()) throw std::runtime_error("Mesh dependency header disagrees with slots");
    for (int32 I = 0; I < Expected.Num(); ++I)
        if (std::string_view(Expected[I].CStr(), Expected[I].Len()) !=
            std::string_view(File.Dependencies[I].CStr(), File.Dependencies[I].Len()))
            throw std::runtime_error("Mesh dependency header disagrees with slots");
    return File;
}
