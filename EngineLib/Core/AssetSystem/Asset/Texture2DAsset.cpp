#include "Texture2DAsset.h"
#include "Core/AssetSystem/AssetSource/FileAssetSource.h"
#include <directxtk/DDSTextureLoader.h>
#include <directxtk/WICTextureLoader.h>
#include <cstring>
#include <stdexcept>

#pragma comment(lib, "ole32.lib")

IMPLEMENT_CLASS(UTexture2DAsset, UAsset);

using Microsoft::WRL::ComPtr;

namespace
{
    // WIC needs COM on the calling thread. An existing STA is also usable.
    struct FScopedCOM
    {
        HRESULT Result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ~FScopedCOM() { if (SUCCEEDED(Result)) CoUninitialize(); }
        bool IsReady() const { return SUCCEEDED(Result) || Result == RPC_E_CHANGED_MODE; }
    };

    void ReportFailure(const FName& Name, HRESULT Result)
    {
        const std::string Message = "Texture asset load failed: " +
            std::string(Name.ToString().CStr()) + " (HRESULT " + std::to_string(Result) + ")\n";
        OutputDebugStringA(Message.c_str());
    }
}

void UTexture2DAsset::Initialize(const FName& InAssetName,
    ComPtr<ID3D11Texture2D> InTexture, ComPtr<ID3D11ShaderResourceView> InSRV)
{
    if (!InTexture || !InSRV) throw std::invalid_argument("Texture and SRV must both be valid");
    D3D11_TEXTURE2D_DESC Desc{};
    InTexture->GetDesc(&Desc);
    D3D11_SHADER_RESOURCE_VIEW_DESC ViewDesc{};
    InSRV->GetDesc(&ViewDesc);
    ComPtr<ID3D11Resource> ViewResource;
    InSRV->GetResource(&ViewResource);
    ComPtr<ID3D11Texture2D> ViewTexture;
    if (FAILED(ViewResource.As(&ViewTexture)) || ViewTexture.Get() != InTexture.Get() ||
        Desc.ArraySize != 1 || Desc.SampleDesc.Count != 1 || ViewDesc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D)
        throw std::invalid_argument("Expected a matching single 2D texture SRV");

    UObject::Initialize();
    SetName(InAssetName);
    Texture = std::move(InTexture);
    SRV = std::move(InSRV);
    Width = Desc.Width;
    Height = Desc.Height;
    MipLevels = Desc.MipLevels;
    Format = Desc.Format;
}

UAsset* FTexture2DAssetLoader::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
    if (!Device)
    {
        ReportFailure(AssetName, E_INVALIDARG);
        return nullptr;
    }

    FString FileContent;
    try
    {
        const auto& FileSource = static_cast<const FFileAssetSource&>(AssetSource);
        FileContent = FileSource.ReadFileToString();
    }
    catch (const std::exception& Error)
    {
        OutputDebugStringA(Error.what());
        ReportFailure(AssetName, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        return nullptr;
    }
    const auto* Data = reinterpret_cast<const uint8_t*>(FileContent.CStr());
    const size_t Size = static_cast<size_t>(FileContent.Len());
    if (!Size)
    {
        ReportFailure(AssetName, HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
        return nullptr;
    }

    ComPtr<ID3D11Resource> Resource;
    ComPtr<ID3D11ShaderResourceView> View;
    HRESULT Result;
    if (Size >= 4 && std::memcmp(Data, "DDS ", 4) == 0)
    {
        // Same DDS path as Week4; preserve compressed format and authored mipmaps.
        Result = DirectX::CreateDDSTextureFromMemory(Device.Get(), Data, Size, &Resource, &View);
    }
    else
    {
        FScopedCOM COM;
        Result = COM.Result;
        if (COM.IsReady())
        {
            // Match Week4's PNG/font path: no sRGB conversion or auto-generated mipmaps.
            Result = DirectX::CreateWICTextureFromMemoryEx(Device.Get(), Data, Size, 0,
                D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
                DirectX::WIC_LOADER_IGNORE_SRGB, &Resource, &View);
        }
    }
    ComPtr<ID3D11Texture2D> Texture;
    if (SUCCEEDED(Result)) Result = Resource.As(&Texture);
    if (FAILED(Result) || !Texture || !View)
    {
        ReportFailure(AssetName, FAILED(Result) ? Result : E_FAIL);
        return nullptr;
    }
    D3D11_TEXTURE2D_DESC Desc{};
    D3D11_SHADER_RESOURCE_VIEW_DESC ViewDesc{};
    Texture->GetDesc(&Desc);
    View->GetDesc(&ViewDesc);
    if (Desc.ArraySize != 1 || Desc.SampleDesc.Count != 1 || ViewDesc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D)
    {
        ReportFailure(AssetName, E_INVALIDARG);
        return nullptr;
    }
    // Register a UObject only after decoding and GPU resource creation succeeded.
    return FObjectFactory::ConstructObject<UTexture2DAsset>(AssetName, std::move(Texture), std::move(View));
}
