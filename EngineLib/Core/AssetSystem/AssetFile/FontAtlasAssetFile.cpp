#include "FontAtlasAssetFile.h"
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace
{
    void Append(TArray<uint8>& Out, const void* Data, size_t Size)
    {
        const size_t Offset = Out.Num();
        if (Size > static_cast<size_t>((std::numeric_limits<int32>::max)()) - Offset)
            throw std::runtime_error("Font asset exceeds supported size");
        Out.SetNum(static_cast<int32>(Offset + Size));
        if (Size) std::memcpy(Out.GetData() + Offset, Data, Size);
    }

    void WriteInteger(TArray<uint8>& Out, uint64 Value, unsigned Size)
    {
        uint8 Bytes[8]{};
        for (unsigned I = 0; I < Size; ++I) Bytes[I] = static_cast<uint8>(Value >> (8 * I));
        Append(Out, Bytes, Size);
    }

    struct FReader
    {
        std::span<const uint8> Bytes;
        std::span<const uint8> Read(uint64 Size)
        {
            if (Size > Bytes.size() || Size > static_cast<uint64>((std::numeric_limits<int32>::max)()))
                throw std::runtime_error("Invalid or truncated font asset body");
            const auto Result = Bytes.first(static_cast<size_t>(Size));
            Bytes = Bytes.subspan(static_cast<size_t>(Size));
            return Result;
        }
        uint64 Integer(unsigned Size)
        {
            const auto Data = Read(Size);
            uint64 Value = 0;
            for (unsigned I = 0; I < Size; ++I) Value |= uint64(Data[I]) << (8 * I);
            return Value;
        }
        float Float() { return std::bit_cast<float>(static_cast<uint32>(Integer(4))); }
    };
}

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
    File.MakeFontResource();
    auto Out = SerializeHeader(File);
    WriteInteger(Out, File.bMSDF ? 1 : 0, 1);
    if (File.bMSDF)
    {
        WriteInteger(Out, File.MetadataJson.Len(), 4);
        Append(Out, File.MetadataJson.CStr(), File.MetadataJson.Len());
    }
    else
    {
        WriteInteger(Out, File.BitmapSettings.Columns, 4);
        WriteInteger(Out, File.BitmapSettings.Rows, 4);
        for (float Value : {File.BitmapSettings.CharacterWidth, File.BitmapSettings.CharacterHeight,
            File.BitmapSettings.CharacterAdvance})
            WriteInteger(Out, std::bit_cast<uint32>(Value), 4);
    }
    WriteInteger(Out, File.Data.Num(), 8);
    Append(Out, File.Data.GetData(), File.Data.Num());
    return Out;
}

FFontAtlas_uasset AssetFile::DeserializeFontAtlas(std::span<const uint8> Bytes)
{
    FFontAtlas_uasset File;
    static_cast<FFile_uasset&>(File) = ReadHeader(Bytes);
    if (std::string_view(File.AssetType.CStr()) != "UFontAtlasAsset")
        throw std::runtime_error("Asset is not UFontAtlasAsset");
    FReader Reader{Bytes};
    const auto Kind = Reader.Integer(1);
    if (Kind > 1) throw std::runtime_error("Invalid font atlas mode");
    File.bMSDF = Kind == 1;
    if (File.bMSDF)
    {
        const auto Json = Reader.Read(Reader.Integer(4));
        File.MetadataJson = FString(std::string_view(reinterpret_cast<const char*>(Json.data()), Json.size()));
    }
    else
    {
        const auto Columns = Reader.Integer(4), Rows = Reader.Integer(4);
        if (Columns > 256 || Rows > 256) throw std::runtime_error("Invalid bitmap grid dimensions");
        File.BitmapSettings.Columns = static_cast<int32>(Columns);
        File.BitmapSettings.Rows = static_cast<int32>(Rows);
        File.BitmapSettings.CharacterWidth = Reader.Float();
        File.BitmapSettings.CharacterHeight = Reader.Float();
        File.BitmapSettings.CharacterAdvance = Reader.Float();
    }
    const auto Data = Reader.Read(Reader.Integer(8));
    if (!Reader.Bytes.empty()) throw std::runtime_error("Trailing font atlas asset data");
    File.Data.SetNum(static_cast<int32>(Data.size()));
    if (!Data.empty()) std::memcpy(File.Data.GetData(), Data.data(), Data.size());
    File.MakeFontResource();
    return File;
}
