#include "Texture2DImporter.h"
#include "AssetImporter.h"
#include "Core/AssetSystem/AssetFile/Texture2DAssetFile.h"
#include "Core/IO/FileManager.h"
#include "Editor/Console.h"
#include "Rendering/Renderer.h"
#include <DirectXTex.h>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace
{
    void Check(HRESULT Result, const char* Operation)
    {
        if (FAILED(Result)) throw std::runtime_error(std::string(Operation) +
            " (HRESULT " + std::to_string(Result) + ")");
    }

    struct FScopedCOM
    {
        HRESULT Result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ~FScopedCOM() { if (SUCCEEDED(Result)) CoUninitialize(); }
    };

    TArray<uint8> CopyBytes(const void* Data, size_t Size)
    {
        if (Size > static_cast<size_t>((std::numeric_limits<int32>::max)()))
            throw std::runtime_error("Texture data exceeds supported asset size");
        TArray<uint8> Result;
        Result.SetNum(static_cast<int32>(Size));
        if (Size) std::memcpy(Result.GetData(), Data, Size);
        return Result;
    }

    TArray<uint8> ConvertToDDS(const uint8* Data, size_t Size, bool bGenerateMipMaps)
    {
        if (Size >= 4 && std::memcmp(Data, "DDS ", 4) == 0) return CopyBytes(Data, Size);

        FScopedCOM COM;
        if (COM.Result != RPC_E_CHANGED_MODE) Check(COM.Result, "Initialize WIC COM");
        DirectX::ScratchImage Image;
        // Keep the previous non-sRGB WIC behavior; normalize imported images to RGBA8.
        Check(DirectX::LoadFromWICMemory(Data, Size,
            DirectX::WIC_FLAGS_FORCE_RGB | DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, Image), "Decode image");

        if (Image.GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM)
        {
            DirectX::ScratchImage RGBA;
            Check(DirectX::Convert(Image.GetImages(), Image.GetImageCount(), Image.GetMetadata(),
                DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, 0.5f, RGBA), "Convert to RGBA8");
            Image = std::move(RGBA);
        }

        DirectX::ScratchImage Mips;
        const DirectX::ScratchImage* CompleteImage = &Image;
        if (bGenerateMipMaps && (Image.GetMetadata().width > 1 || Image.GetMetadata().height > 1))
        {
            // levels=0 generates the complete chain through 1x1, including non-power-of-two images.
            Check(DirectX::GenerateMipMaps(Image.GetImages(), Image.GetImageCount(), Image.GetMetadata(),
                DirectX::TEX_FILTER_DEFAULT, 0, Mips), "Generate texture mipmaps");
            CompleteImage = &Mips;
        }
        DirectX::Blob DDS;
        Check(DirectX::SaveToDDSMemory(CompleteImage->GetImages(), CompleteImage->GetImageCount(),
            CompleteImage->GetMetadata(), DirectX::DDS_FLAGS_NONE, DDS), "Encode DDS");
        return CopyBytes(DDS.GetBufferPointer(), DDS.GetBufferSize());
    }
}

FTexture2D_uasset FTexture2DImporter::PrepareTexture2D(URenderer& Renderer,
    const std::filesystem::path& SourcePath, bool bStandalone, bool bGenerateMipMaps)
{
    const auto Content = FFileManager::Get().ReadFileToString(SourcePath);
    if (!Content.Len()) throw std::runtime_error("Texture source is empty");
    FTexture2D_uasset File;
    File.bStandalone = bStandalone;
    File.Data = ConvertToDDS(reinterpret_cast<const uint8*>(Content.CStr()), Content.Len(), bGenerateMipMaps);

    // Probe the exact DDS bytes that will be saved. Release GPU resources before writing files.
    {
        auto Texture = Renderer.CreateTexture2DFromMemory(File.Data.GetData(), File.Data.Num());
        if (!Texture) throw std::runtime_error("Renderer could not create the imported DDS texture");
        D3D11_TEXTURE2D_DESC Desc{};
        Texture->GetDesc(&Desc);
        if (Desc.ArraySize != 1 || Desc.SampleDesc.Count != 1 || (Desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE))
            throw std::runtime_error("Texture2D import does not support arrays or cubemaps");
    }

    return File;
}

TArray<FName> FTexture2DImporter::ImportUTexture2D(URenderer& Renderer, const std::filesystem::path& SourcePath,
    const std::filesystem::path& Destination, bool bStandalone)
{
    try
    {
        auto& Files = FFileManager::Get();
        const auto Source = Files.ResolvePath(SourcePath);
        auto Target = Files.ResolvePath(Destination.empty() ? std::filesystem::path("Textures") : Destination);
        if (Target.extension() != ".uasset") Target /= Source.stem().wstring() + L".uasset";
        if (std::filesystem::exists(Target)) throw std::runtime_error("Texture asset destination already exists");

        const auto File = PrepareTexture2D(Renderer, Source, bStandalone);
        const auto Bytes = AssetFile::Serialize(File);
        return WriteImportedAsset(Target, {Bytes.GetData(), static_cast<size_t>(Bytes.Num())});
    }
    catch (const std::exception& Error)
    {
        UE_DEBUG_LOG_ERROR(Core, "Texture2D import failed: %s", Error.what());
        return {};
    }
}
