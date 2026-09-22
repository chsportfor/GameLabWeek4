#include "AssetFile.h"
#include <algorithm>
#include <cstring>
#include <istream>
#include <limits>
#include <stdexcept>

namespace
{
    void AppendString(TArray<uint8>& Out, const FString& Value)
    {
        const uint32 Size = Value.Len();
        const size_t Offset = Out.Num();
        if (uint64(Offset) + 4 + Size > uint64((std::numeric_limits<int32>::max)()))
            throw std::runtime_error("Asset header exceeds supported size");
        Out.SetNum(static_cast<int32>(Offset + 4 + Size));
        for (unsigned I = 0; I < 4; ++I) Out[static_cast<uint32>(Offset + I)] = uint8(Size >> (I * 8));
        if (Size) std::memcpy(Out.GetData() + Offset + 4, Value.CStr(), Size);
    }

    struct FStreamReader
    {
        std::istream& Stream;
        void Read(void* Destination, size_t Size)
        {
            if (!Stream.read(static_cast<char*>(Destination), static_cast<std::streamsize>(Size)))
                throw std::runtime_error("Cannot read complete asset header");
        }
    };

    struct FMemoryReader
    {
        std::span<const uint8> Bytes;
        void Read(void* Destination, size_t Size)
        {
            if (Size > Bytes.size()) throw std::runtime_error("Truncated asset header");
            if (Size) std::memcpy(Destination, Bytes.data(), Size);
            Bytes = Bytes.subspan(Size);
        }
    };

    template<typename TReader>
    uint32 ReadUInt32(TReader& Reader)
    {
        uint8 Bytes[4];
        Reader.Read(Bytes, sizeof(Bytes));
        return uint32(Bytes[0]) | (uint32(Bytes[1]) << 8) |
            (uint32(Bytes[2]) << 16) | (uint32(Bytes[3]) << 24);
    }

    template<typename TReader>
    FString ReadString(TReader& Reader)
    {
        uint32 Remaining = ReadUInt32(Reader);
        if (Remaining > uint32((std::numeric_limits<int32>::max)()))
            throw std::runtime_error("Asset header string exceeds supported size");
        std::string Value;
        // Read before allocating: a corrupt length must not cause a huge upfront allocation.
        char Buffer[4096];
        while (Remaining)
        {
            const auto Size = (std::min)(Remaining, uint32(sizeof(Buffer)));
            Reader.Read(Buffer, Size);
            Value.append(Buffer, Size);
            Remaining -= Size;
        }
        return FString(Value);
    }

    template<typename TReader>
    FFile_uasset ParseHeader(TReader& Reader)
    {
        char Magic[4];
        Reader.Read(Magic, sizeof(Magic));
        if (std::memcmp(Magic, "UAST", 4) || ReadUInt32(Reader) != 1)
            throw std::runtime_error("Unsupported asset file signature/version");
        FFile_uasset Header;
        Header.AssetType = ReadString(Reader);
        if (!Header.AssetType.Len()) throw std::runtime_error("Empty asset class in header");
        uint8 Standalone;
        Reader.Read(&Standalone, sizeof(Standalone));
        if (Standalone > 1) throw std::runtime_error("Invalid standalone flag");
        Header.bStandalone = Standalone != 0;
        const auto Count = ReadUInt32(Reader);
        if (Count > uint32((std::numeric_limits<int32>::max)()))
            throw std::runtime_error("Invalid dependency count");
        for (uint32 I = 0; I < Count; ++I) Header.Dependencies.Add(ReadString(Reader));
        return Header;
    }
}

TArray<uint8> AssetFile::SerializeHeader(const FFile_uasset& Header)
{
    if (!Header.AssetType.Len()) throw std::runtime_error("Empty asset class in header");
    TArray<uint8> Out = {'U', 'A', 'S', 'T', 1, 0, 0, 0};
    AppendString(Out, Header.AssetType);
    Out.Add(Header.bStandalone ? 1 : 0);
    const uint32 Count = Header.Dependencies.Num();
    for (unsigned I = 0; I < 4; ++I) Out.Add(uint8(Count >> (I * 8)));
    for (const auto& Path : Header.Dependencies) AppendString(Out, Path);
    return Out;
}

FFile_uasset AssetFile::ReadHeader(std::istream& Stream)
{
    FStreamReader Reader{Stream};
    return ParseHeader(Reader);
}

FFile_uasset AssetFile::ReadHeader(std::span<const uint8>& Bytes)
{
    FMemoryReader Reader{Bytes};
    auto Header = ParseHeader(Reader);
    Bytes = Reader.Bytes;
    return Header;
}
