#include "FontAtlasAssetFile.h"
#include "AssetFileJson.h"
#include "AssetFileDocument.h"
#include <cstring>
#include <stdexcept>

FFontResource FFontAtlas_uasset::MakeFontResource() const
{
    if (std::string_view(AssetType.CStr()) != "UFontAtlasAsset" || !Dependencies.IsEmpty())
        throw std::runtime_error("Expected a self-contained UFontAtlasAsset");
    if (Data.Num() < 4 || std::memcmp(Data.GetData(), "DDS ", 4) != 0)
        throw std::runtime_error("Font atlas requires embedded DDS bytes");
    if (bMSDF)
    {
        FFontResource Font;
        if (std::string_view(MetadataJson.CStr(), MetadataJson.Len()).find('\0') != std::string_view::npos ||
            !Font.LoadUnicodeAtlasFromString(MetadataJson))
            throw std::runtime_error("Invalid embedded MSDF font metadata");
        return Font;
    }
    if (MetadataJson.Len() || !BitmapSettings.IsValid())
        throw std::runtime_error("Invalid bitmap font grid settings");
    return FFontResource(BitmapSettings.Columns, BitmapSettings.Rows, BitmapSettings.CharacterWidth,
        BitmapSettings.CharacterHeight, BitmapSettings.CharacterAdvance);
}

TArray<uint8> AssetFile::Serialize(const FFontAtlas_uasset& File)
{
    using namespace Detail;
    File.MakeFontResource();
    Json Body{"Mode", File.bMSDF ? "MSDF" : "Bitmap", "Image", ImageDescriptor(File.Data.Num())};
    if (File.bMSDF)
    {
        Body["Metadata"] = Json::Load(std::string(File.MetadataJson.CStr(), File.MetadataJson.Len()));
        Object(Body.at("Metadata"));
    }
    else
    {
        const auto& S = File.BitmapSettings;
        Body["BitmapSettings"] = Json{"Columns", S.Columns, "Rows", S.Rows,
            "CharacterWidth", S.CharacterWidth, "CharacterHeight", S.CharacterHeight,
            "CharacterAdvance", S.CharacterAdvance};
    }
    FFile_uasset Header = File;
    Header.SchemaVersion = GetFontAtlasFileSchema().LatestVersion;
    return WriteDocument(Header, Body, {File.Data.GetData(), size_t(File.Data.Num())});
}
FFontAtlas_uasset AssetFile::DeserializeFontAtlas(std::span<const uint8> Bytes)
{
    using namespace Detail;
    FFontAtlas_uasset File;
    const auto Body = ReadDocument(Bytes, File, "UFontAtlasAsset");
    if (File.SchemaVersion != GetFontAtlasFileSchema().LatestVersion)
        throw std::runtime_error("Asset requires schema upgrade before loading");
    const auto Mode = String(Field(Body, "Mode"));
    if (Mode != "MSDF" && Mode != "Bitmap") throw std::runtime_error("Unsupported font atlas mode");
    File.bMSDF = Mode == "MSDF";
    if (File.bMSDF)
    {
        const auto& Metadata = Field(Body, "Metadata"); Object(Metadata);
        File.MetadataJson = FString(Metadata.dump());
    }
    else if (Body.hasKey("BitmapSettings"))
    {
        const auto& S = Body.at("BitmapSettings"); Object(S);
        if (S.hasKey("Columns")) File.BitmapSettings.Columns = UInt(S.at("Columns"));
        if (S.hasKey("Rows")) File.BitmapSettings.Rows = UInt(S.at("Rows"));
        if (S.hasKey("CharacterWidth")) File.BitmapSettings.CharacterWidth = Float(S.at("CharacterWidth"));
        if (S.hasKey("CharacterHeight")) File.BitmapSettings.CharacterHeight = Float(S.at("CharacterHeight"));
        if (S.hasKey("CharacterAdvance")) File.BitmapSettings.CharacterAdvance = Float(S.at("CharacterAdvance"));
    }
    File.Data = ReadImage(Body, Bytes);
    File.MakeFontResource();
    return File;
}

void AssetFile::UpgradeFontAtlasToLatest(FAssetFileDocument& Document)
{
    // JSON schema starts at v1: no conversion yet. When introducing v2, raise
    // LatestVersion below and add a 1->2 step; keep old steps for future versions.
    // v2 example: record the old spacing behavior as an explicit new setting.
    // if (Document.Header.SchemaVersion == 1) {
    //     if (!Document.Body.hasKey("LineSpacingScale")) Document.Body["LineSpacingScale"] = 1.0f;
    //     Document.Header.SchemaVersion = 2;
    // } // Keep Metadata (including unknown fields) and atlas DDS unchanged.
    (void)Document;
}

const FAssetFileSchema& AssetFile::GetFontAtlasFileSchema()
{
    static const FAssetFileSchema Schema{
        1, // LatestVersion: the only latest-version constant for this asset type.
        &UpgradeFontAtlasToLatest,
        [](FAssetFileDocument& Document)
        {
            Document.Header.Dependencies.Empty();
        },
        [](const FAssetFileDocument& Document)
        {
            const auto Bytes = Detail::WriteDocument(Document.Header, Document.Body,
                {Document.Payload.GetData(), size_t(Document.Payload.Num())});
            (void)DeserializeFontAtlas({Bytes.GetData(), size_t(Bytes.Num())});
        }
    };
    return Schema;
}
