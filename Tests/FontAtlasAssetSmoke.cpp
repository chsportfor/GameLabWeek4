#include "Core/AssetSystem/Asset/FontAtlasAsset.h"
#include "Core/AssetSystem/AssetSource/FontAtlasAssetSource.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

static void Check(bool Value, const char* Message)
{
    if (!Value) throw std::runtime_error(Message);
}

static void SameGlyph(const FCharacterInfo* A, const FCharacterInfo* B)
{
    Check(A && B, "Glyph missing");
    Check(A->U == B->U && A->V == B->V && A->UVWidth == B->UVWidth && A->UVHeight == B->UVHeight &&
        A->Width == B->Width && A->Height == B->Height && A->Advance == B->Advance &&
        A->BearingX == B->BearingX && A->BearingY == B->BearingY &&
        A->AdvanceX == B->AdvanceX && A->HasGeometry == B->HasGeometry, "Glyph conversion changed");
}

int main()
{
    try
    {
        Microsoft::WRL::ComPtr<ID3D11Device> Device;
        Check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &Device, nullptr, nullptr)), "WARP device");
        FFileManager Files("EngineLib/Assets");
        FFontAtlasAssetLoader Loader(Device.Get());
        FFontAtlasAssetSource Bitmap(Files, "Fonts/EnglishBigFontAtlas.dds");
        UAsset* Base = Loader.LoadAsset(FName("ASCII"), Bitmap);
        Check(Base && Base->IsA<UFontAtlasAsset>() && Base->IsA<UTexture2DAsset>(), "Bitmap load and RTTI");
        auto* ASCII = Base->Cast<UFontAtlasAsset>();
        FFontResource Reference;
        for (int I = 0; I < 256; ++I)
            SameGlyph(ASCII->FindCharacter(static_cast<char>(I)), Reference.FindCharacter(static_cast<char>(I)));
        Check(!ASCII->IsMSDF() && ASCII->GetSRV(), "Bitmap kind/texture");
        ASCII->Destroy();

        FFontAtlasAssetSource MSDF(Files, "Fonts/KoreanFullAtlas.png", "Fonts/KoreanFullAtlas.json");
        Base = Loader.LoadAsset(FName("Korean"), MSDF);
        Check(Base != nullptr, "MSDF load");
        auto* Korean = Base->Cast<UFontAtlasAsset>();
        Check(Korean && Korean->IsMSDF() && Korean->GetWidth() == 4096 && Korean->GetHeight() == 4096
            && Korean->GetDistanceRange() == 4 && Korean->GetMipLevels() == 1, "MSDF metadata");
        Check(Reference.LoadUnicodeAtlas(FString("EngineLib/Assets/Fonts/KoreanFullAtlas.json")), "Legacy path API");
        for (uint32 Code : { 32u, 65u, 0xAC00u, 0xD7A3u })
            SameGlyph(Korean->FindUnicodeCharacter(Code), Reference.FindUnicodeCharacter(Code));
        Check(!Korean->FindUnicodeCharacter(32)->HasGeometry, "Space geometry");
        Check(!Korean->FindUnicodeCharacter(0x10FFFF), "Missing glyph");
        Loader.UnloadAsset(Korean);
        Check(Korean->GetSRV() != nullptr, "Loader must not destroy a referenced asset");
        Korean->Destroy();

        FBitmapFontAtlasSettings InvalidGrid;
        InvalidGrid.Columns = 0;
        FFontAtlasAssetSource BadGrid(Files, "Fonts/EnglishBigFontAtlas.dds", InvalidGrid);
        Check(!Loader.LoadAsset(FName("BadGrid"), BadGrid), "Invalid grid");
        FFontAtlasAssetSource Mismatch(Files, "Fonts/EnglishFont.png", "Fonts/KoreanFullAtlas.json");
        Check(!Loader.LoadAsset(FName("Mismatch"), Mismatch), "Image/JSON dimensions");
        FFontAtlasAssetSource Missing(Files, "Fonts/KoreanFullAtlas.png", "Fonts/missing.json");
        Check(!Loader.LoadAsset(FName("Missing"), Missing), "Missing JSON");
        FFontAtlasAssetSource MissingImage(Files, "Fonts/missing.png", "Fonts/KoreanFullAtlas.json");
        Check(!Loader.LoadAsset(FName("MissingImage"), MissingImage), "Missing image");

        const auto JsonPath = std::filesystem::absolute("Tests/bin-texture/invalid-font.json");
        FFontAtlasAssetSource BadJSON(Files, "Fonts/EnglishFont.png", JsonPath);
        for (const char* JSON : {
            "{}",
            R"({"atlas":{"type":"msdf","yOrigin":"top","width":512,"height":512,"distanceRange":0},"glyphs":[{"unicode":32,"advance":1}]})",
            R"({"atlas":{"type":"msdf","yOrigin":"top","width":512,"height":512,"distanceRange":4},"glyphs":[{"unicode":32,"advance":1},{"unicode":32,"advance":1}]})",
            R"({"atlas":{"type":"msdf","yOrigin":"top","width":512,"height":512,"distanceRange":4},"glyphs":[{"unicode":55296,"advance":1}]})" })
        {
            { std::ofstream File(JsonPath); File << JSON; }
            Check(!Loader.LoadAsset(FName("BadJSON"), BadJSON), "Invalid MSDF metadata");
        }
        Check(!Reference.LoadUnicodeAtlasFromString(FString("{}")), "Invalid metadata rejected");
        Check(Reference.FindUnicodeCharacter(0xAC00) && Reference.GetDistanceRange() == 4,
            "Failed reload should preserve old metadata");
        std::cout << "PASS: ASCII 256 glyphs, Korean MSDF, legacy API, RTTI, lifetime and invalid input\n";
    }
    catch (const std::exception& Error)
    {
        std::cerr << "FAIL: " << Error.what() << '\n';
        return 1;
    }
}
