#include "Texture2DAssetFile.h"
#include <cstring>
#include <limits>
#include <stdexcept>

namespace
{
    void Append(TArray<uint8>& Out, const void* Data, size_t Size)
    {
        const size_t Offset = Out.Num();
        if (Size > static_cast<size_t>((std::numeric_limits<int32>::max)()) - Offset)
            throw std::runtime_error("Asset file exceeds supported size");
        Out.SetNum(static_cast<int32>(Offset + Size));
        if (Size) std::memcpy(Out.GetData() + Offset, Data, Size);
    }

    void WriteInteger(TArray<uint8>& Out, uint64 Value, unsigned Bytes)
    {
        for (unsigned I = 0; I < Bytes; ++I) Out.Add(static_cast<uint8>(Value >> (8 * I)));
    }

    struct FReader
    {
        std::span<const uint8> Bytes;
        std::span<const uint8> Read(size_t Size)
        {
            if (Size > Bytes.size()) throw std::runtime_error("Truncated asset file");
            const auto Result = Bytes.first(Size);
            Bytes = Bytes.subspan(Size);
            return Result;
        }
        uint64 Integer(unsigned Size)
        {
            const auto Data = Read(Size);
            uint64 Result = 0;
            for (unsigned I = 0; I < Size; ++I) Result |= uint64(Data[I]) << (8 * I);
            return Result;
        }
    };
}

TArray<uint8> AssetFile::Serialize(const FTexture2D_uasset& File)
{
    if (std::string_view(File.AssetType.CStr()) != "UTexture2D" || File.Data.Num() < 4 ||
        std::memcmp(File.Data.GetData(), "DDS ", 4) != 0)
        throw std::runtime_error("Expected UTexture2D metadata and DDS payload");
    TArray<uint8> Out = SerializeHeader(File);
    WriteInteger(Out, File.Data.Num(), 8);
    Append(Out, File.Data.GetData(), File.Data.Num());
    return Out;
}

FTexture2D_uasset AssetFile::DeserializeTexture2D(std::span<const uint8> Bytes)
{
    FTexture2D_uasset File;
    static_cast<FFile_uasset&>(File) = ReadHeader(Bytes);
    if (std::string_view(File.AssetType.CStr()) != "UTexture2D")
        throw std::runtime_error("Asset is not UTexture2D");
    FReader Reader{Bytes};
    const auto Size = Reader.Integer(8);
    if (Size < 4 || Size > static_cast<uint64>((std::numeric_limits<int32>::max)()))
        throw std::runtime_error("Invalid texture payload size");
    const auto Data = Reader.Read(static_cast<size_t>(Size));
    if (!Reader.Bytes.empty() || std::memcmp(Data.data(), "DDS ", 4))
        throw std::runtime_error("Invalid DDS payload or trailing asset data");
    File.Data.SetNum(static_cast<int32>(Size));
    std::memcpy(File.Data.GetData(), Data.data(), Data.size());
    return File;
}
