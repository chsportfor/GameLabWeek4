#include "AssetFileJson.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <istream>
#include <limits>
#include <set>
#include <stdexcept>

namespace AssetFile::Detail
{
    void Object(const Json& Value)
    {
        if (Value.JSONType() != Json::Class::Object) throw std::runtime_error("Expected JSON object");
    }
    void Array(const Json& Value)
    {
        if (Value.JSONType() != Json::Class::Array) throw std::runtime_error("Expected JSON array");
    }
    const Json& Field(const Json& Value, const char* Name)
    {
        Object(Value);
        if (!Value.hasKey(Name)) throw std::runtime_error(std::string("Missing asset field: ") + Name);
        return Value.at(Name);
    }
    std::string String(const Json& Value) { return Value.ToRawString(); }
    uint32 UInt(const Json& Value)
    {
        if (Value.JSONType() != Json::Class::Integral || Value.ToInt() < 0 ||
            uint64(Value.ToInt()) > uint64((std::numeric_limits<int32>::max)()))
            throw std::runtime_error("Expected non-negative asset integer within INT32_MAX");
        return static_cast<uint32>(Value.ToInt());
    }
    float Float(const Json& Value)
    {
        double Number;
        if (Value.JSONType() == Json::Class::Integral) Number = Value.ToInt();
        else if (Value.JSONType() == Json::Class::Floating) Number = Value.ToFloat();
        else throw std::runtime_error("Expected asset number");
        if (!std::isfinite(Number) || std::abs(Number) > (std::numeric_limits<float>::max)())
            throw std::runtime_error("Non-finite or out-of-range asset number");
        return static_cast<float>(Number);
    }
    Json FloatArray(std::initializer_list<float> Values)
    {
        Json Result = json::Array();
        for (float Value : Values) Result.append(Value);
        return Result;
    }
    void ReadFloats(const Json& Value, std::span<float> Destination)
    {
        Array(Value);
        if (Value.length() != Destination.size()) throw std::runtime_error("Incorrect asset vector length");
        for (size_t I = 0; I < Destination.size(); ++I) Destination[I] = Float(Value.at(static_cast<unsigned>(I)));
    }
    void Append(TArray<uint8>& Out, std::span<const uint8> Bytes)
    {
        const size_t Offset = Out.Num();
        if (Bytes.size() > size_t((std::numeric_limits<int32>::max)()) - Offset)
            throw std::runtime_error("Asset exceeds supported file size");
        Out.SetNum(static_cast<int32>(Offset + Bytes.size()));
        if (!Bytes.empty()) std::memcpy(Out.GetData() + Offset, Bytes.data(), Bytes.size());
    }
    TArray<uint8> WriteDocument(const FFile_uasset& Header, const Json& Body, std::span<const uint8> Payload)
    {
        Object(Body);
        const auto Text = Body.dump();
        auto Out = SerializeHeader(Header, Text.size());
        Append(Out, {reinterpret_cast<const uint8*>(Text.data()), Text.size()});
        Append(Out, Payload);
        return Out;
    }
    Json ReadDocument(std::span<const uint8>& Bytes, FFile_uasset& Header, const char* ExpectedClass)
    {
        if (Bytes.size() > size_t((std::numeric_limits<int32>::max)())) throw std::runtime_error("Asset too large");
        uint64 BodySize;
        Header = ReadHeader(Bytes, &BodySize);
        if (std::string_view(Header.AssetType.CStr()) != ExpectedClass) throw std::runtime_error("Unexpected asset class");
        if (BodySize > Bytes.size()) throw std::runtime_error("Truncated asset JSON body");
        auto Body = Json::Load(std::string(reinterpret_cast<const char*>(Bytes.data()), static_cast<size_t>(BodySize)));
        Object(Body);
        Bytes = Bytes.subspan(static_cast<size_t>(BodySize));
        return Body;
    }
    Json ImageDescriptor(size_t ByteLength)
    {
        if (ByteLength > size_t((std::numeric_limits<int32>::max)())) throw std::runtime_error("Image too large");
        return Json{"Encoding", "DDS", "Offset", 0, "ByteLength", static_cast<int32>(ByteLength)};
    }
    void ValidateImage(std::span<const uint8> Payload)
    {
        if (Payload.size() < 4 || std::memcmp(Payload.data(), "DDS ", 4)) throw std::runtime_error("Expected DDS payload");
    }
    TArray<uint8> ReadImage(const Json& Body, std::span<const uint8> Payload)
    {
        const auto& Image = Field(Body, "Image");
        if (String(Field(Image, "Encoding")) != "DDS" || UInt(Field(Image, "Offset")) != 0 ||
            UInt(Field(Image, "ByteLength")) != Payload.size())
            throw std::runtime_error("Invalid DDS descriptor or trailing data");
        ValidateImage(Payload);
        TArray<uint8> Result; Append(Result, Payload); return Result;
    }
}

namespace
{
    using namespace AssetFile::Detail;
    void Integer(TArray<uint8>& Out, uint64 Value, unsigned Count)
    {
        for (unsigned I = 0; I < Count; ++I) Out.Add(uint8(Value >> (I * 8)));
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
    template<typename TReader> uint64 Integer(TReader& Reader, unsigned Count)
    {
        uint8 Bytes[8]{}; Reader.Read(Bytes, Count);
        uint64 Result = 0;
        for (unsigned I = 0; I < Count; ++I) Result |= uint64(Bytes[I]) << (I * 8);
        return Result;
    }
    template<typename TReader> FFile_uasset ParseHeader(TReader& Reader, uint64* BodyLength)
    {
        char Magic[4]; Reader.Read(Magic, 4);
        if (std::memcmp(Magic, "UAJS", 4) || Integer(Reader, 4) != 1)
            throw std::runtime_error("Unsupported asset container (expected UAJS version 1)");
        const uint64 H = Integer(Reader, 8), B = Integer(Reader, 8);
        if (H > uint64((std::numeric_limits<int32>::max)()) ||
            B > uint64((std::numeric_limits<int32>::max)()) || 24 + H + B > uint64((std::numeric_limits<int32>::max)()))
            throw std::runtime_error("Asset JSON length exceeds supported size");
        std::string Text;
        char Buffer[4096];
        for (uint64 Remaining = H; Remaining;)
        {
            const auto Size = static_cast<size_t>((std::min)(Remaining, uint64(sizeof(Buffer))));
            Reader.Read(Buffer, Size); Text.append(Buffer, Size); Remaining -= Size;
        }
        const auto Header = Json::Load(Text);
        FFile_uasset Result;
        Result.SchemaVersion = UInt(Field(Header, "SchemaVersion"));
        if (!Result.SchemaVersion) throw std::runtime_error("Asset schema version must be positive");
        const auto Type = String(Field(Header, "AssetType"));
        if (Type.empty() || Type.find('\0') != std::string::npos) throw std::runtime_error("Invalid asset class name");
        Result.AssetType = FString(Type);
        const auto& Standalone = Field(Header, "Standalone");
        if (Standalone.JSONType() != Json::Class::Boolean) throw std::runtime_error("Invalid standalone flag");
        Result.bStandalone = Standalone.ToBool();
        const auto& Dependencies = Field(Header, "Dependencies"); Array(Dependencies);
        std::set<std::string> Seen;
        for (const auto& Value : Dependencies.ArrayRange())
        {
            const auto Path = String(Value);
            if (Path.empty() || Path.find('\0') != std::string::npos || !Seen.insert(Path).second)
                throw std::runtime_error("Invalid or duplicate asset dependency");
            Result.Dependencies.Add(FString(Path));
        }
        if (BodyLength) *BodyLength = B;
        return Result;
    }
}

TArray<uint8> AssetFile::SerializeHeader(const FFile_uasset& Header, uint64 BodyByteLength)
{
    using namespace Detail;
    if (!Header.AssetType.Len()) throw std::runtime_error("Empty asset class");
    if (!Header.SchemaVersion || Header.SchemaVersion > uint32((std::numeric_limits<int32>::max)()))
        throw std::runtime_error("Invalid asset schema version");
    Json Dependencies = json::Array();
    std::set<std::string> Seen;
    for (const auto& Path : Header.Dependencies)
    {
        const std::string Text(Path.CStr(), Path.Len());
        if (Text.empty() || Text.find('\0') != std::string::npos || !Seen.insert(Text).second)
            throw std::runtime_error("Invalid or duplicate dependency");
        Dependencies.append(Text);
    }
    Json Object{"AssetType", std::string(Header.AssetType.CStr(), Header.AssetType.Len()),
        "SchemaVersion", Header.SchemaVersion, "Standalone", Header.bStandalone, "Dependencies", Dependencies};
    const auto Text = Object.dump();
    if (BodyByteLength > uint64((std::numeric_limits<int32>::max)()) ||
        24 + Text.size() + BodyByteLength > uint64((std::numeric_limits<int32>::max)()))
        throw std::runtime_error("Asset header too large");
    TArray<uint8> Out{'U', 'A', 'J', 'S'};
    Integer(Out, 1, 4); Integer(Out, Text.size(), 8); Integer(Out, BodyByteLength, 8);
    Append(Out, {reinterpret_cast<const uint8*>(Text.data()), Text.size()});
    return Out;
}
FFile_uasset AssetFile::ReadHeader(std::istream& Stream, uint64* BodyByteLength)
{
    FStreamReader Reader{Stream}; return ParseHeader(Reader, BodyByteLength);
}
FFile_uasset AssetFile::ReadHeader(std::span<const uint8>& Bytes, uint64* BodyByteLength)
{
    FMemoryReader Reader{Bytes}; auto Result = ParseHeader(Reader, BodyByteLength);
    Bytes = Reader.Bytes; return Result;
}
