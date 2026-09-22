#include "AssetPropertyPanel.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/IO/FileManager.h"
#include "ThirdParty/Json/json.hpp"
#include "ThirdParty/ImGui/imgui.h"
#include <DirectXTex.h>
#include <algorithm>
#include <fstream>

namespace fs = std::filesystem;
namespace
{
    std::string Utf8(const fs::path& Path)
    {
        const auto Text = Path.generic_u8string();
        return {reinterpret_cast<const char*>(Text.data()), Text.size()};
    }
    std::string Value(const json::JSON& Json)
    {
        if (Json.JSONType() == json::JSON::Class::String) return Json.ToRawString();
        return Json.dump();
    }
    void Names(const char* Label, const TArray<FName>& Values)
    {
        ImGui::SeparatorText(Label);
        std::vector<std::string> Sorted;
        for (const auto& Name : Values) Sorted.emplace_back(Name.ToString().CStr());
        std::sort(Sorted.begin(), Sorted.end());
        ImGui::TextDisabled("%zu asset(s)", Sorted.size());
        for (const auto& Name : Sorted) ImGui::TextWrapped("%s", Name.c_str());
    }
}

void FAssetPropertyPanel::Inspect(const fs::path& RelativePath, const FFileManager& Files)
{
    Path = RelativePath;
    Name = FName(Utf8(Path).c_str());
    Header = {}; Details.clear(); Error.clear(); FileBytes = 0;
    try
    {
        const auto File = Files.ResolvePath(Path);
        if (Path.empty() || Path.is_absolute() || !IsUnder(File, Files.GetFileDirectoryPath()))
            throw std::runtime_error("Asset must be inside Assets.");
        FileBytes = fs::file_size(File);
        std::ifstream Stream(File, std::ios::binary);
        uint64 BodyLength = 0;
        Header = AssetFile::ReadHeader(Stream, &BodyLength);
        if (Stream.tellg() < 0 || uint64(Stream.tellg()) + BodyLength > FileBytes)
            throw std::runtime_error("Truncated asset description.");
        std::string Text(size_t(BodyLength), '\0');
        if (!Stream.read(Text.data(), std::streamsize(BodyLength)))
            throw std::runtime_error("Cannot read asset description.");
        const auto Body = json::JSON::Load(Text);
        if (Body.JSONType() != json::JSON::Class::Object) throw std::runtime_error("Invalid asset description.");
        auto Field = [&](const char* Label, const json::JSON& Object, const char* Key)
        {
            if (Object.hasKey(Key)) Details.emplace_back(Label, Value(Object.at(Key)));
        };
        if (Body.hasKey("DiffuseColor"))
        {
            Field("Diffuse RGBA", Body, "DiffuseColor");
            Field("Diffuse texture", Body, "DiffuseTexture");
        }
        if (Body.hasKey("Geometry"))
        {
            const auto& Geometry = Body.at("Geometry");
            Field("Vertex layout", Geometry, "VertexLayout");
            if (Geometry.hasKey("Vertices")) Field("Vertices", Geometry.at("Vertices"), "Count");
            if (Geometry.hasKey("Indices")) Field("Indices", Geometry.at("Indices"), "Count");
        }
        if (Body.hasKey("Bounds"))
        {
            Field("Bounds min", Body.at("Bounds"), "Min");
            Field("Bounds max", Body.at("Bounds"), "Max");
        }
        if (Body.hasKey("Sections"))
        {
            const auto& Sections = Body.at("Sections");
            Details.emplace_back("Sections", std::to_string(Sections.length()));
            unsigned Index = 0;
            for (const auto& Section : Sections.ArrayRange())
                Details.emplace_back("Section " + std::to_string(Index++), Value(Section));
        }
        if (Body.hasKey("MaterialPaths"))
        {
            unsigned Index = 0;
            for (const auto& Material : Body.at("MaterialPaths").ArrayRange())
                Details.emplace_back("Material slot " + std::to_string(Index++), Value(Material));
        }
        Field("Atlas mode", Body, "Mode");
        if (Body.hasKey("BitmapSettings"))
        {
            const auto& Settings = Body.at("BitmapSettings");
            for (const auto* Key : {"Columns", "Rows", "CharacterWidth", "CharacterHeight", "CharacterAdvance"})
                Field(Key, Settings, Key);
        }
        if (Body.hasKey("Metadata"))
        {
            const auto& Metadata = Body.at("Metadata");
            if (Metadata.hasKey("glyphs")) Details.emplace_back("Glyphs", std::to_string(Metadata.at("glyphs").length()));
            if (Metadata.hasKey("atlas")) Field("Distance range", Metadata.at("atlas"), "distanceRange");
        }
        if (Body.hasKey("Image"))
        {
            Field("Image encoding", Body.at("Image"), "Encoding");
            Field("Image bytes", Body.at("Image"), "ByteLength");
            // DDS metadata needs only its header (including the optional DX10 header).
            uint8 DDS[148]{};
            Stream.read(reinterpret_cast<char*>(DDS), sizeof(DDS));
            DirectX::TexMetadata Metadata{};
            if (SUCCEEDED(DirectX::GetMetadataFromDDSMemory(DDS, size_t(Stream.gcount()), DirectX::DDS_FLAGS_NONE, Metadata)))
            {
                Details.emplace_back("Resolution", std::to_string(Metadata.width) + " x " + std::to_string(Metadata.height));
                Details.emplace_back("Mip levels", std::to_string(Metadata.mipLevels));
                Details.emplace_back("DXGI format", std::to_string(unsigned(Metadata.format)));
            }
            else Details.emplace_back("Image metadata", "Invalid or unavailable DDS header");
        }
    }
    catch (const std::exception& Ex) { Error = Ex.what(); }
}

bool FAssetPropertyPanel::ValidateSelection(const UAssetManager& Assets)
{
    if (Name.IsValid() && Assets.FindMetaInfo(Name)) return true;
    // Registration, not the lifetime of a retired loaded object, defines selection validity.
    Path.clear(); Name = {}; Header = {}; FileBytes = 0;
    Details.clear(); Error.clear();
    return false;
}

void FAssetPropertyPanel::Draw(const FFileManager& Files, UAssetManager& Assets)
{
    ImGui::SeparatorText("Asset");
    if (ImGui::SmallButton("Refresh info")) Refresh(Files);
    ImGui::TextWrapped("Name: %s", Utf8(Path.stem()).c_str());
    ImGui::TextWrapped("Path: %s", Utf8(Path).c_str());
    const auto* Meta = Assets.FindMetaInfo(Name);
    ImGui::TextWrapped("Class: %s", Meta ? Meta->AssetClass->Name.CStr() : Header.AssetType.CStr());
    ImGui::Text("File size: %llu bytes", static_cast<unsigned long long>(FileBytes));
    if (Header.AssetType.Len()) ImGui::Text("Schema version: %u", Header.SchemaVersion);
    if (Meta || Header.AssetType.Len())
        ImGui::Text("Standalone: %s", (Meta ? Meta->bStandalone : Header.bStandalone) ? "Yes" : "No");
    ImGui::Text("Registered: %s", Meta ? "Yes" : "No");
    ImGui::Text("Loaded: %s", Assets.GetAsset(Name, false) ? "Yes" : "No");
    if (!Error.empty()) ImGui::TextWrapped("File information: %s", Error.c_str());
    if (!Details.empty())
    {
        ImGui::SeparatorText("File properties (read-only)");
        for (const auto& [Label, Text] : Details) ImGui::TextWrapped("%s: %s", Label.c_str(), Text.c_str());
    }
    TArray<FName> Dependencies;
    if (Meta) Dependencies = Meta->Dependencies;
    else for (const auto& Dependency : Header.Dependencies) Dependencies.Add(FName(Dependency));
    Names(Meta ? "Dependencies" : "Dependencies (file header)", Dependencies);
    Names("Direct referencers", Assets.GetReferencers(Name));
    ImGui::TextWrapped("Registered asset files only; scene references are not tracked.");
}
