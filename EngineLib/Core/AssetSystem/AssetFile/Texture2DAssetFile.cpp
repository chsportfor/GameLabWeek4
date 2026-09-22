#include "Texture2DAssetFile.h"
#include "AssetFileJson.h"
#include "AssetFileDocument.h"
#include <stdexcept>

TArray<uint8> AssetFile::Serialize(const FTexture2D_uasset& File)
{
    using namespace Detail;
    if (std::string_view(File.AssetType.CStr()) != "UTexture2D" || !File.Dependencies.IsEmpty())
        throw std::runtime_error("Expected a self-contained UTexture2D");
    const std::span<const uint8> Data{File.Data.GetData(), size_t(File.Data.Num())};
    ValidateImage(Data);
    FFile_uasset Header = File;
    Header.SchemaVersion = GetTexture2DFileSchema().LatestVersion;
    return WriteDocument(Header, Json{"Image", ImageDescriptor(Data.size())}, Data);
}
FTexture2D_uasset AssetFile::DeserializeTexture2D(std::span<const uint8> Bytes)
{
    FTexture2D_uasset File;
    const auto Body = Detail::ReadDocument(Bytes, File, "UTexture2D");
    if (File.SchemaVersion != GetTexture2DFileSchema().LatestVersion)
        throw std::runtime_error("Asset requires schema upgrade before loading");
    if (!File.Dependencies.IsEmpty()) throw std::runtime_error("Texture cannot have dependencies");
    File.Data = Detail::ReadImage(Body, Bytes);
    return File;
}

void AssetFile::UpgradeTexture2DToLatest(FAssetFileDocument& Document)
{
    // JSON schema starts at v1: no conversion yet. When introducing v2, raise
    // LatestVersion below and add a 1->2 step; keep old steps for future versions.
    // v2 example: preserve the old linear interpretation when introducing sRGB.
    // if (Document.Header.SchemaVersion == 1) {
    //     if (!Document.Body.hasKey("SRGB")) Document.Body["SRGB"] = false;
    //     Document.Header.SchemaVersion = 2;
    // } // DDS bytes remain untouched; update Serialize/Deserialize for SRGB too.
    (void)Document;
}

const FAssetFileSchema& AssetFile::GetTexture2DFileSchema()
{
    static const FAssetFileSchema Schema{
        1, // LatestVersion: the only latest-version constant for this asset type.
        &UpgradeTexture2DToLatest,
        [](FAssetFileDocument& Document)
        {
            Document.Header.Dependencies.Empty();
        },
        [](const FAssetFileDocument& Document)
        {
            const auto Bytes = Detail::WriteDocument(Document.Header, Document.Body,
                {Document.Payload.GetData(), size_t(Document.Payload.Num())});
            (void)DeserializeTexture2D({Bytes.GetData(), size_t(Bytes.Num())});
        }
    };
    return Schema;
}
