#pragma once
#include "AssetFile.h"
#include "ThirdParty/Json/json.hpp"

// Shared file IO details; class-specific field mapping remains in each serializer.
namespace AssetFile::Detail
{
    using Json = json::JSON;
    const Json& Field(const Json& Object, const char* Name);
    void Object(const Json& Value);
    void Array(const Json& Value);
    std::string String(const Json& Value);
    uint32 UInt(const Json& Value);
    float Float(const Json& Value);
    Json FloatArray(std::initializer_list<float> Values);
    void ReadFloats(const Json& Value, std::span<float> Destination);
    void Append(TArray<uint8>& Out, std::span<const uint8> Bytes);
    TArray<uint8> WriteDocument(const FFile_uasset& Header, const Json& Body,
        std::span<const uint8> Payload = {});
    Json ReadDocument(std::span<const uint8>& Bytes, FFile_uasset& Header, const char* ExpectedClass);
    Json ImageDescriptor(size_t ByteLength);
    TArray<uint8> ReadImage(const Json& Body, std::span<const uint8> Payload);
    void ValidateImage(std::span<const uint8> Payload);
}
