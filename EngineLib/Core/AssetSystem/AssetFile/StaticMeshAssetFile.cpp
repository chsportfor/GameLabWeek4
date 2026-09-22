#include "StaticMeshAssetFile.h"
#include "AssetFileJson.h"
#include "AssetFileDocument.h"
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

    float ReadFloat(std::span<const uint8>& Bytes) { return std::bit_cast<float>(ReadUInt32(Bytes)); }
}

TArray<uint8> AssetFile::Serialize(const FStaticMesh_uasset& File)
{
    using namespace Detail;
    Validate(File);
    FFile_uasset Header = File;
    Header.SchemaVersion = GetStaticMeshFileSchema().LatestVersion;
    Header.Dependencies = Dependencies(File);
    const uint64 VertexBytes = uint64(File.Geometry.Vertices.Num()) * 48;
    const uint64 IndexBytes = uint64(File.Geometry.Indices.Num()) * 4;
    if (VertexBytes + IndexBytes > uint64((std::numeric_limits<int32>::max)()))
        throw std::runtime_error("Mesh payload too large");
    TArray<uint8> Payload; Payload.Reserve(static_cast<int32>(VertexBytes + IndexBytes));
    for (const auto& V : File.Geometry.Vertices)
        for (float Value : VertexValues(V)) WriteUInt32(Payload, std::bit_cast<uint32>(Value));
    for (uint32 Index : File.Geometry.Indices) WriteUInt32(Payload, Index);
    Json Sections = json::Array(), Materials = json::Array();
    for (const auto& S : File.Sections)
        Sections.append(Json{"FirstIndex", S.FirstIndex, "IndexCount", S.IndexCount, "MaterialIndex", S.MaterialIndex});
    for (const auto& Path : File.MaterialPaths) Materials.append(std::string(Path.CStr(), Path.Len()));
    Json Body{
        "Bounds", Json{"Min", FloatArray({File.Bounds.Min.x, File.Bounds.Min.y, File.Bounds.Min.z}),
                       "Max", FloatArray({File.Bounds.Max.x, File.Bounds.Max.y, File.Bounds.Max.z})},
        "MaterialPaths", Materials, "Sections", Sections,
        "Geometry", Json{"VertexLayout", "VertexSimpleV1",
            "Vertices", Json{"Offset", 0, "Count", File.Geometry.Vertices.Num(), "ByteLength", static_cast<int32>(VertexBytes)},
            "IndexFormat", "UInt32",
            "Indices", Json{"Offset", static_cast<int32>(VertexBytes), "Count", File.Geometry.Indices.Num(),
                "ByteLength", static_cast<int32>(IndexBytes)}}};
    return WriteDocument(Header, Body, {Payload.GetData(), size_t(Payload.Num())});
}

FStaticMesh_uasset AssetFile::DeserializeStaticMesh(std::span<const uint8> Bytes)
{
    using namespace Detail;
    FStaticMesh_uasset File;
    const auto Body = ReadDocument(Bytes, File, "UStaticMeshAsset");
    if (File.SchemaVersion != GetStaticMeshFileSchema().LatestVersion)
        throw std::runtime_error("Asset requires schema upgrade before loading");
    const auto& Geometry = Field(Body, "Geometry");
    if (String(Field(Geometry, "VertexLayout")) != "VertexSimpleV1" ||
        String(Field(Geometry, "IndexFormat")) != "UInt32") throw std::runtime_error("Unsupported mesh binary layout");
    const auto& Vertices = Field(Geometry, "Vertices");
    const auto& Indices = Field(Geometry, "Indices");
    const uint32 VertexCount = UInt(Field(Vertices, "Count")), IndexCount = UInt(Field(Indices, "Count"));
    const uint64 VertexBytes = uint64(VertexCount) * 48, IndexBytes = uint64(IndexCount) * 4;
    if (!VertexCount || !IndexCount || UInt(Field(Vertices, "Offset")) != 0 ||
        UInt(Field(Vertices, "ByteLength")) != VertexBytes || UInt(Field(Indices, "Offset")) != VertexBytes ||
        UInt(Field(Indices, "ByteLength")) != IndexBytes || VertexBytes + IndexBytes != Bytes.size())
        throw std::runtime_error("Invalid mesh payload offsets, sizes or counts");
    File.Geometry.Vertices.SetNum(VertexCount);
    for (auto& V : File.Geometry.Vertices)
    {
        std::array<float, 12> Values;
        for (auto& Value : Values) Value = ReadFloat(Bytes);
        V = {Values[0], Values[1], Values[2], Values[3], Values[4], Values[5],
            Values[6], Values[7], Values[8], Values[9], Values[10], Values[11]};
    }
    File.Geometry.Indices.SetNum(IndexCount);
    for (auto& Index : File.Geometry.Indices) Index = ReadUInt32(Bytes);
    const auto& Bounds = Field(Body, "Bounds");
    float Min[3], Max[3]; ReadFloats(Field(Bounds, "Min"), Min); ReadFloats(Field(Bounds, "Max"), Max);
    File.Bounds.Min = {Min[0], Min[1], Min[2]}; File.Bounds.Max = {Max[0], Max[1], Max[2]};
    const auto& Sections = Field(Body, "Sections"); Array(Sections);
    for (const auto& Section : Sections.ArrayRange())
        File.Sections.Add(FMeshSection{UInt(Field(Section, "FirstIndex")), UInt(Field(Section, "IndexCount")),
            UInt(Field(Section, "MaterialIndex"))});
    const auto& Materials = Field(Body, "MaterialPaths"); Array(Materials);
    for (const auto& Path : Materials.ArrayRange()) File.MaterialPaths.Add(FString(String(Path)));
    Validate(File);
    const auto Expected = Dependencies(File);
    if (Expected.Num() != File.Dependencies.Num()) throw std::runtime_error("Mesh dependency header disagrees with slots");
    // Dependency order has no meaning; material-slot order does.
    std::set<std::string> Actual;
    for (const auto& Path : File.Dependencies) Actual.emplace(Path.CStr(), Path.Len());
    for (const auto& Path : Expected)
        if (!Actual.contains(std::string(Path.CStr(), Path.Len())))
            throw std::runtime_error("Mesh dependency header disagrees with slots");
    return File;
}

void AssetFile::UpgradeStaticMeshToLatest(FAssetFileDocument& Document)
{
    // JSON schema starts at v1: no conversion yet. When introducing v2, raise
    // LatestVersion below and add a 1->2 step; keep old steps for future versions.
    // v2 example: introduce an asset setting without touching the old geometry.
    // if (Document.Header.SchemaVersion == 1) {
    //     if (!Document.Body.hasKey("CastShadow")) Document.Body["CastShadow"] = true;
    //     Document.Header.SchemaVersion = 2;
    // }
    // For a vertex-format change, decode the OLD layout, rebuild Payload, then
    // update Geometry.VertexLayout/offsets/counts/lengths before advancing version.
    (void)Document;
}

const FAssetFileSchema& AssetFile::GetStaticMeshFileSchema()
{
    static const FAssetFileSchema Schema{
        1, // LatestVersion: the only latest-version constant for this asset type.
        &UpgradeStaticMeshToLatest,
        [](FAssetFileDocument& Document)
        {
            Document.Header.Dependencies.Empty();
            const auto& Paths = Detail::Field(Document.Body, "MaterialPaths");
            Detail::Array(Paths);
            std::set<std::string> Seen;
            for (const auto& Value : Paths.ArrayRange())
            {
                const auto Path = Detail::String(Value);
                if (Seen.insert(Path).second) Document.Header.Dependencies.Add(FString(Path));
            }
        },
        [](const FAssetFileDocument& Document)
        {
            const auto Bytes = Detail::WriteDocument(Document.Header, Document.Body,
                {Document.Payload.GetData(), size_t(Document.Payload.Num())});
            (void)DeserializeStaticMesh({Bytes.GetData(), size_t(Bytes.Num())});
        }
    };
    return Schema;
}
