#include "MaterialAssetFile.h"
#include "AssetFileJson.h"
#include "AssetFileDocument.h"
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

}

TArray<uint8> AssetFile::Serialize(const FMaterial_uasset& File)
{
    using namespace Detail;
    Validate(File);
    FFile_uasset Header = File;
    Header.SchemaVersion = GetMaterialFileSchema().LatestVersion;
    Header.Dependencies.Empty();
    Json Texture;
    if (File.DiffuseTexturePath.Len())
    {
        Header.Dependencies.Add(File.DiffuseTexturePath);
        Texture = std::string(File.DiffuseTexturePath.CStr(), File.DiffuseTexturePath.Len());
    }
    return WriteDocument(Header, Json{"DiffuseColor", FloatArray({File.DiffuseColor.R,
        File.DiffuseColor.G, File.DiffuseColor.B, File.DiffuseColor.A}), "DiffuseTexture", Texture});
}
FMaterial_uasset AssetFile::DeserializeMaterial(std::span<const uint8> Bytes)
{
    using namespace Detail;
    FMaterial_uasset File;
    const auto Body = ReadDocument(Bytes, File, "UMaterial");
    if (File.SchemaVersion != GetMaterialFileSchema().LatestVersion)
        throw std::runtime_error("Asset requires schema upgrade before loading");
    if (!Bytes.empty()) throw std::runtime_error("Material cannot have binary payload");
    if (Body.hasKey("DiffuseColor"))
    {
        float Color[4]; ReadFloats(Body.at("DiffuseColor"), Color);
        File.DiffuseColor = FLinearColor{Color[0], Color[1], Color[2], Color[3]};
    }
    if (Body.hasKey("DiffuseTexture") && !Body.at("DiffuseTexture").IsNull())
        File.DiffuseTexturePath = FString(String(Body.at("DiffuseTexture")));
    Validate(File);
    const int32 ExpectedCount = File.DiffuseTexturePath.Len() ? 1 : 0;
    if (File.Dependencies.Num() != ExpectedCount || (ExpectedCount &&
        std::string_view(File.Dependencies[0].CStr(), File.Dependencies[0].Len()) !=
        std::string_view(File.DiffuseTexturePath.CStr(), File.DiffuseTexturePath.Len())))
        throw std::runtime_error("Material dependency header disagrees with body references");
    return File;
}

void AssetFile::UpgradeMaterialToLatest(FAssetFileDocument& Document)
{
    // JSON schema starts at v1: no conversion yet. When introducing v2, raise
    // LatestVersion below and add a 1->2 step; keep old steps for future versions.
    // v2 example: explicitly preserve old appearance when adding Roughness.
    // if (Document.Header.SchemaVersion == 1) {
    //     if (!Document.Body.hasKey("Roughness")) Document.Body["Roughness"] = 0.5f;
    //     Document.Header.SchemaVersion = 2;
    // } // Also teach Serialize/Deserialize to write/read Roughness.
    (void)Document;
}

const FAssetFileSchema& AssetFile::GetMaterialFileSchema()
{
    static const FAssetFileSchema Schema{
        1, // LatestVersion: the only latest-version constant for this asset type.
        &UpgradeMaterialToLatest,
        [](FAssetFileDocument& Document)
        {
            Document.Header.Dependencies.Empty();
            if (Document.Body.hasKey("DiffuseTexture") && !Document.Body.at("DiffuseTexture").IsNull())
            {
                const auto Path = Detail::String(Document.Body.at("DiffuseTexture"));
                if (!Path.empty()) Document.Header.Dependencies.Add(FString(Path));
            }
        },
        [](const FAssetFileDocument& Document)
        {
            const auto Bytes = Detail::WriteDocument(Document.Header, Document.Body,
                {Document.Payload.GetData(), size_t(Document.Payload.Num())});
            (void)DeserializeMaterial({Bytes.GetData(), size_t(Bytes.Num())});
        }
    };
    return Schema;
}
