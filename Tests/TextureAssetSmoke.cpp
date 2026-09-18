#include "Core/AssetSystem/Asset/Texture2DAsset.h"
#include "Core/AssetSystem/AssetSource/FileAssetSource.h"
#include <iostream>
#include <stdexcept>
#include <fstream>

static void Check(bool Value, const char* Message)
{
    if (!Value) throw std::runtime_error(Message);
}

int main()
{
    try
    {
        Microsoft::WRL::ComPtr<ID3D11Device> Device;
        Check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &Device, nullptr, nullptr)), "Create WARP device");
        FTexture2DAssetLoader Loader(Device.Get());
        FFileManager Files("EngineLib/Assets");
        for (const char* Path : { "Textures/CubeTextureSample.dds", "Textures/EarthTexture.dds",
            "Textures/Explosion_Alpha.dds", "Textures/LoadingScreen.dds", "Fonts/EnglishFont.png" })
        {
            FFileAssetSource Source(Files, Path);
            auto Base = Loader.LoadAsset(FName(Path), Source);
            Check(Base != nullptr, "Texture load");
            auto Asset = std::static_pointer_cast<FTexture2DAsset>(Base);
            Check(Asset->GetName() == FName(Path), "Asset name");
            D3D11_TEXTURE2D_DESC Desc{};
            Asset->GetTexture()->GetDesc(&Desc);
            Check(Asset->GetSRV() && Desc.Width == Asset->GetWidth() && Desc.Height == Asset->GetHeight()
                && Desc.Format == Asset->GetFormat() && Desc.MipLevels == Asset->GetMipLevels(), "Asset metadata");
            std::cout << Path << ": " << Desc.Width << 'x' << Desc.Height << ", mips=" << Desc.MipLevels << ", format=" << Desc.Format << '\n';

            Asset.reset();
        }
        FFileAssetSource Missing(Files, "missing-texture.dds");
        Check(!Loader.LoadAsset(FName("Missing"), Missing), "Missing file");
        const auto InvalidPath = std::filesystem::absolute("Tests/bin-texture/invalid-image.bin");
        FFileAssetSource Invalid(Files, InvalidPath);
        for (const char* Data : { "", "not an image", "DDS invalid" })
        {
            { std::ofstream File(InvalidPath, std::ios::binary | std::ios::trunc); File << Data; }
            Check(!Loader.LoadAsset(FName("InvalidFile"), Invalid), "Invalid/empty file data");
        }
        FTexture2DAssetLoader NoDevice(nullptr);
        Check(!NoDevice.LoadAsset(FName("NoDevice"), Invalid), "Null device");

        // Exercise WIC when COM is already initialized in a different apartment.
        HRESULT COM = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        Check(SUCCEEDED(COM), "Initialize STA");
        FFileAssetSource PNG(Files, "Fonts/EnglishFont.png");
        auto Asset = Loader.LoadAsset(FName("STA"), PNG);
        Check(Asset != nullptr, "WIC on existing STA");
        Asset.reset();
        CoUninitialize();
        std::cout << "PASS: DDS, WIC, metadata, shared lifetime, COM and failure paths\n";
    }
    catch (const std::exception& Error)
    {
        std::cerr << "FAIL: " << Error.what() << '\n';
        return 1;
    }
}
