#include "ContentBrowser.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/AssetSystem/Asset/Texture2DAsset.h"
#include "Core/IO/FileManager.h"
#include "Engine/Assets/Importers/Texture2DImporter.h"
#include "Engine/Assets/Importers/MaterialImporter.h"
#include "Engine/Assets/Importers/FontAtlasImporter.h"
#include "Editor/Console.h"
#include "ThirdParty/ImGui/imgui.h"
#include <commdlg.h>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;
namespace
{
    std::string Utf8(const fs::path& Path)
    {
        const auto Text = Path.generic_u8string();
        return {reinterpret_cast<const char*>(Text.data()), Text.size()};
    }
    void ValidateName(std::string_view Name)
    {
        if (Name.empty() || Name == "." || Name == ".." || Name.back() == '.' || Name.back() == ' ' ||
            Name.find_first_of("<>:\"/\\|?*") != std::string_view::npos ||
            std::any_of(Name.begin(), Name.end(), [](unsigned char C) { return C < 32; }))
            throw std::runtime_error("Enter a filename without path separators or reserved characters.");
        auto Base = std::string(Name.substr(0, Name.find('.')));
        std::transform(Base.begin(), Base.end(), Base.begin(), [](unsigned char C) { return char(std::toupper(C)); });
        if (Base == "CON" || Base == "PRN" || Base == "AUX" || Base == "NUL" ||
            (Base.size() == 4 && (Base.starts_with("COM") || Base.starts_with("LPT")) && Base[3] >= '1' && Base[3] <= '9'))
            throw std::runtime_error("This name is reserved by Windows.");
    }
    constexpr const wchar_t* ImageFilter = L"Image files\0*.png;*.jpg;*.jpeg;*.dds;*.bmp;*.tif;*.tiff;*.gif\0All files\0*.*\0";
    // Source files can live outside Assets. Only imported output is restricted to Assets.
    bool SourceInput(const char* Label, char (&Buffer)[4096], const wchar_t* Filter)
    {
        ImGui::PushID(Label);
        ImGui::TextUnformatted(Label);
        ImGui::SetNextItemWidth((std::max)(60.0f, ImGui::GetContentRegionAvail().x - 90));
        bool Changed = ImGui::InputText("##Path", Buffer, sizeof(Buffer));
        ImGui::SameLine();
        if (ImGui::Button("Browse..."))
        {
            wchar_t Path[32768]{};
            OPENFILENAMEW Dialog{};
            Dialog.lStructSize = sizeof(Dialog);
            Dialog.hwndOwner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
            Dialog.lpstrFilter = Filter;
            Dialog.lpstrFile = Path;
            Dialog.nMaxFile = DWORD(std::size(Path));
            Dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameW(&Dialog))
            {
                const auto Text = Utf8(Path);
                if (Text.size() < sizeof(Buffer)) { strcpy_s(Buffer, Text.c_str()); Changed = true; }
                else UE_LOG(Error, Editor, "Selected source path is too long.");
            }
        }
        ImGui::PopID();
        return Changed;
    }
}

void FContentBrowser::AddFolder(const FFileManager& Files, const UAssetManager& Assets)
{
    try
    {
        auto Path = CurrentDirectory / "New Folder";
        for (unsigned I = 1; fs::exists(Root / Path); ++I)
            Path = CurrentDirectory / ("New Folder " + std::to_string(I));
        if (!IsUnder(Root / Path, Root)) throw std::runtime_error("Folder must be inside Assets.");
        if (!fs::create_directory(Root / Path)) throw std::runtime_error("Cannot create folder.");
        Refresh(Files, Assets);
        SelectedPath = NamingFolder = Path;
        strcpy_s(FolderName, Utf8(Path.filename()).c_str());
        FocusFolderName = true;
    }
    catch (const std::exception& Ex) { Error = Ex.what(); UE_LOG(Error, Editor, "Add Folder: %s", Ex.what()); }
}

void FContentBrowser::CommitFolderName(const FFileManager& Files, const UAssetManager& Assets)
{
    try
    {
        ValidateName(FolderName);
        const auto Target = NamingFolder.parent_path() / fs::u8path(FolderName);
        if (!IsUnder(Root / NamingFolder, Root) || !IsUnder(Root / Target, Root))
            throw std::runtime_error("Folder must remain inside Assets.");
        if (Target != NamingFolder)
        {
            if (fs::exists(Root / Target)) throw std::runtime_error("A file or folder already has that name.");
            fs::rename(Root / NamingFolder, Root / Target);
        }
        NamingFolder.clear();
        Refresh(Files, Assets);
        SelectedPath = Target;
    }
    catch (const std::exception& Ex)
    {
        Error = Ex.what(); FocusFolderName = true;
        UE_LOG(Error, Editor, "Name Folder: %s", Ex.what());
    }
}

void FContentBrowser::OpenCreation()
{
    CreationDirectory = CurrentDirectory;
    SourcePath[0] = MetadataPath[0] = AssetName[0] = 0;
    TexturePath.clear(); CreateError.clear();
    std::fill(std::begin(DiffuseColor), std::end(DiffuseColor), 1.0f);
    BitmapSettings = {};
    ImGui::OpenPopup("Add Asset");
}

bool FContentBrowser::CreateAsset(const FFileManager& Files, UAssetManager& Assets, URenderer& Renderer)
{
    try
    {
        const bool Mtl = CreateType == ECreateType::Material && MaterialMode == 0;
        const auto Directory = Root / CreationDirectory;
        if (!IsUnder(Directory, Root) || !fs::is_directory(Directory))
            throw std::runtime_error("The destination folder no longer exists inside Assets.");
        fs::path Target;
        if (!Mtl)
        {
            ValidateName(AssetName);
            Target = Directory / (fs::u8path(AssetName).wstring() + L".uasset");
            if (fs::exists(Target)) throw std::runtime_error("An asset with that name already exists.");
        }
        if (!(CreateType == ECreateType::Material && MaterialMode == 2) && !SourcePath[0])
            throw std::runtime_error("Choose a source file.");
        const auto Source = fs::u8path(SourcePath);
        const FLinearColor Color{DiffuseColor[0], DiffuseColor[1], DiffuseColor[2], DiffuseColor[3]};
        TArray<FName> Created;
        // User-created top-level assets are Standalone; importers own dependency policy.
        switch (CreateType)
        {
        case ECreateType::Texture:
            Created = FTexture2DImporter::ImportUTexture2D(Renderer, Source, Target, true); break;
        case ECreateType::Material:
            if (Mtl) Created = FMaterialImporter::ImportUMaterial(Renderer, Source, Directory, true);
            else if (MaterialMode == 1)
                Created = FMaterialImporter::ImportUMaterialFromImage(Renderer, Source, Color, Target, true);
            else Created = FMaterialImporter::ImportUMaterial(fs::u8path(TexturePath), Color, Target, true);
            break;
        case ECreateType::Font:
            if (FontMode == 0)
            {
                if (!MetadataPath[0]) throw std::runtime_error("Choose MSDF metadata JSON.");
                Created = FFontAtlasImporter::ImportUFontAtlas(Renderer, Source, fs::u8path(MetadataPath), Target, true);
            }
            else Created = FFontAtlasImporter::ImportUFontAtlas(Renderer, Source, BitmapSettings, Target, true);
            break;
        }
        if (Created.IsEmpty()) throw std::runtime_error("Import failed. See the console for details. No existing assets were overwritten.");
        bool Registered = true;
        for (const auto& Name : Created)
            if (!Assets.RegisterAsset(fs::u8path(Name.ToString().CStr()))) Registered = false;
        Refresh(Files, Assets);
        SelectedPath = Mtl ? fs::u8path(Created[Created.Num() - 1].ToString().CStr()) : Target.lexically_relative(Root);
        if (!Registered) Error = "Files were created, but registration failed. See the console and register again after fixing the error.";
        // Files already exist even if registration failed. Close rather than offer to import duplicates.
        return true;
    }
    catch (const std::exception& Ex)
    {
        CreateError = Ex.what();
        UE_LOG(Error, Editor, "Add Asset: %s", Ex.what());
        return false;
    }
}

void FContentBrowser::DrawCreation(const FFileManager& Files, UAssetManager& Assets, URenderer& Renderer)
{
    ImGui::SetNextWindowSize(ImVec2(760, 510), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(570, 390), ImVec2(FLT_MAX, FLT_MAX));
    bool Open = true;
    if (!ImGui::BeginPopupModal("Add Asset", &Open)) return;
    const float FooterHeight = ImGui::GetFrameHeightWithSpacing() * 2;
    ImGui::BeginChild("Asset Types", ImVec2(158, -FooterHeight), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("Asset Type"); ImGui::Separator();
    auto Type = [&](const char* Label, ECreateType Value)
    {
        if (ImGui::Selectable(Label, CreateType == Value)) { CreateType = Value; CreateError.clear(); }
    };
    Type("Texture2D", ECreateType::Texture);
    Type("Material", ECreateType::Material);
    if (CreateType == ECreateType::Material)
    {
        ImGui::Indent(10);
        const char* Modes[] = {"From .mtl", "From image file", "From Texture2D"};
        for (int I = 0; I < 3; ++I)
            if (ImGui::Selectable(Modes[I], MaterialMode == I)) { MaterialMode = I; CreateError.clear(); }
        ImGui::Unindent(10);
    }
    const bool OpenMeshImport = ImGui::Selectable("StaticMesh", false);
    Type("FontAtlas", ECreateType::Font);
    ImGui::EndChild();
    if (OpenMeshImport)
    {
        StaticMeshImportFolder = CreationDirectory;
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::SameLine();
    ImGui::BeginChild("Creation Details", ImVec2(0, -FooterHeight), ImGuiChildFlags_Borders);
    const bool Mtl = CreateType == ECreateType::Material && MaterialMode == 0;
    const bool ExistingTexture = CreateType == ECreateType::Material && MaterialMode == 2;
    if (CreateType == ECreateType::Font)
    {
        ImGui::Combo("Atlas type", &FontMode, "MSDF image + JSON\0Bitmap grid\0");
        ImGui::Spacing();
    }
    if (ExistingTexture)
    {
        // Header metadata only: opening a picker never loads textures or GPU resources.
        ImGui::TextUnformatted("Texture2D asset");
        const auto Preview = TexturePath.empty() ? std::string("None (color only)") : Utf8(fs::u8path(TexturePath).stem());
        ImGui::SetNextItemWidth((std::max)(60.0f, ImGui::GetContentRegionAvail().x - 90));
        if (ImGui::BeginCombo("##Texture", Preview.c_str()))
        {
            if (ImGui::Selectable("None (color only)", TexturePath.empty())) TexturePath.clear();
            for (const auto& [Folder, Entries] : Folders)
                for (const auto& Entry : Entries)
                    if (Entry.AssetClass == UTexture2D::GetClass() && Assets.FindMetaInfo(FName(Utf8(Entry.Path).c_str())))
                    {
                        const auto Path = Utf8(Entry.Path);
                        if (ImGui::Selectable(Path.c_str(), Path == TexturePath)) TexturePath = Path;
                    }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse..."))
        {
            TextureBrowseDirectory = TexturePath.empty() ? CreationDirectory : fs::u8path(TexturePath).parent_path();
            TextureBrowseSelection = TexturePath;
            ImGui::OpenPopup("Choose Texture2D");
        }
        ImGui::SetNextWindowSize(ImVec2(480, 330), ImGuiCond_FirstUseEver);
        if (ImGui::BeginPopupModal("Choose Texture2D", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::BeginDisabled(TextureBrowseDirectory.empty());
            if (ImGui::Button("Up")) TextureBrowseDirectory = TextureBrowseDirectory.parent_path();
            ImGui::EndDisabled(); ImGui::SameLine();
            if (ImGui::Button("Assets")) TextureBrowseDirectory.clear();
            ImGui::TextWrapped("Assets/%s", Utf8(TextureBrowseDirectory).c_str());
            ImGui::BeginChild("Texture files", ImVec2(450, 235), ImGuiChildFlags_Borders);
            if (const auto It = Folders.find(TextureBrowseDirectory); It != Folders.end())
                for (const auto& Entry : It->second)
                    if (Entry.IsDirectory || (Entry.AssetClass == UTexture2D::GetClass() && Assets.FindMetaInfo(FName(Utf8(Entry.Path).c_str()))))
                    {
                        const auto Path = Utf8(Entry.Path);
                        ImGui::PushID(Path.c_str());
                        const auto Label = (Entry.IsDirectory ? "[Folder] " : "") + Entry.Name;
                        if (ImGui::Selectable(Label.c_str(), Path == TextureBrowseSelection,
                            ImGuiSelectableFlags_AllowDoubleClick))
                        {
                            if (!Entry.IsDirectory) TextureBrowseSelection = Path;
                            else if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) TextureBrowseDirectory = Entry.Path;
                        }
                        ImGui::PopID();
                    }
            ImGui::EndChild();
            ImGui::TextWrapped("Selected: %s", TextureBrowseSelection.empty() ? "None" : TextureBrowseSelection.c_str());
            ImGui::BeginDisabled(TextureBrowseSelection.empty());
            if (ImGui::Button("Select")) { TexturePath = TextureBrowseSelection; ImGui::CloseCurrentPopup(); }
            ImGui::EndDisabled();
            ImGui::SameLine(); if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    else
    {
        const wchar_t* Filter = Mtl ? L"Material library\0*.mtl\0All files\0*.*\0" : ImageFilter;
        if (SourceInput(Mtl ? "Source path (.mtl)" : "Source path (.png, .jpg, .dds, ...)", SourcePath, Filter) && !AssetName[0])
        {
            const auto Stem = Utf8(fs::u8path(SourcePath).stem());
            if (Stem.size() < sizeof(AssetName)) strcpy_s(AssetName, Stem.c_str());
        }
    }
    ImGui::Spacing(); ImGui::Spacing();
    if (!Mtl)
    {
        ImGui::TextUnformatted("Asset name");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##AssetName", AssetName, sizeof(AssetName));
    }
    else ImGui::TextWrapped("Creates one material per newmtl entry, using the names in the MTL file.");
    ImGui::TextWrapped("Folder: Assets/%s", Utf8(CreationDirectory).c_str());
    if (CreateType == ECreateType::Material && !Mtl)
    {
        ImGui::Spacing(); ImGui::SeparatorText("Properties");
        ImGui::ColorEdit4("Diffuse RGBA", DiffuseColor, ImGuiColorEditFlags_Float);
    }
    if (CreateType == ECreateType::Material && !ExistingTexture)
        ImGui::TextWrapped("New textures are saved in Assets/Textures. Existing filenames are not overwritten.");
    if (CreateType == ECreateType::Font)
    {
        ImGui::SeparatorText("Font properties");
        if (FontMode == 0)
        {
            SourceInput("MSDF metadata (.json)", MetadataPath, L"JSON\0*.json\0All files\0*.*\0");
            ImGui::TextWrapped("msdf-atlas-gen: type = msdf, yOrigin = top. Image and metadata are stored in one asset.");
        }
        else
        {
            ImGui::InputInt("Columns", &BitmapSettings.Columns);
            ImGui::InputInt("Rows", &BitmapSettings.Rows);
            ImGui::InputFloat("Character width", &BitmapSettings.CharacterWidth);
            ImGui::InputFloat("Character height", &BitmapSettings.CharacterHeight);
            ImGui::InputFloat("Character advance", &BitmapSettings.CharacterAdvance);
        }
    }
    if (!CreateError.empty()) { ImGui::Separator(); ImGui::TextWrapped("%s", CreateError.c_str()); }
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - 190));
    if (ImGui::Button("Cancel", ImVec2(80, 0))) ImGui::CloseCurrentPopup();
    ImGui::SameLine();
    if (ImGui::Button("Create", ImVec2(80, 0)) && CreateAsset(Files, Assets, Renderer)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
