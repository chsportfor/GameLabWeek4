#include "ContentBrowser.h"

#include "AssetDragDrop.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/AssetSystem/Asset/FontAtlasAsset.h"
#include "Core/AssetSystem/Asset/Material.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/Asset/Texture2DAsset.h"
#include "Core/IO/FileManager.h"
#include "ThirdParty/ImGui/imgui.h"

#include <algorithm>

namespace fs = std::filesystem;

namespace
{
    std::string Utf8(const fs::path& Path)
    {
        const auto Text = Path.generic_u8string();
        return {reinterpret_cast<const char*>(Text.data()), Text.size()};
    }

    enum class EIcon { Folder, Mesh, Material, Texture, Font, Unknown };

    EIcon IconFor(const FClassInfo* Class)
    {
        if (Class == UStaticMeshAsset::GetClass()) return EIcon::Mesh;
        if (Class == UMaterial::GetClass()) return EIcon::Material;
        if (Class == UTexture2D::GetClass()) return EIcon::Texture;
        if (Class == UFontAtlasAsset::GetClass()) return EIcon::Font;
        return EIcon::Unknown;
    }

    const char* TypeLabel(EIcon Icon)
    {
        switch (Icon)
        {
        case EIcon::Folder: return "Folder";
        case EIcon::Mesh: return "Static Mesh";
        case EIcon::Material: return "Material";
        case EIcon::Texture: return "Texture 2D";
        case EIcon::Font: return "Font Atlas";
        default: return "Unknown Asset";
        }
    }

    // Normalized coordinates keep all type symbols readable at different tile sizes.
    void DrawIcon(ImDrawList& Draw, EIcon Icon, ImVec2 Origin, float Size)
    {
        auto P = [&](float X, float Y) { return ImVec2(Origin.x + X * Size, Origin.y + Y * Size); };
        const float Stroke = 2.0f;
        switch (Icon)
        {
        case EIcon::Folder:
            Draw.AddRectFilled(P(.10f, .25f), P(.47f, .43f), IM_COL32(191, 143, 51, 255), 3);
            Draw.AddRectFilled(P(.10f, .35f), P(.90f, .80f), IM_COL32(231, 183, 78, 255), 4);
            break;
        case EIcon::Mesh:
        {
            const ImU32 Color = IM_COL32(85, 183, 243, 255);
            const ImVec2 Top[] = {P(.5f, .12f), P(.87f, .32f), P(.5f, .52f), P(.13f, .32f)};
            Draw.AddConvexPolyFilled(Top, 4, IM_COL32(51, 100, 138, 255));
            Draw.AddLine(P(.5f, .12f), P(.87f, .32f), Color, Stroke);
            Draw.AddLine(P(.5f, .12f), P(.13f, .32f), Color, Stroke);
            Draw.AddLine(P(.13f, .32f), P(.5f, .52f), Color, Stroke);
            Draw.AddLine(P(.87f, .32f), P(.5f, .52f), Color, Stroke);
            Draw.AddLine(P(.13f, .32f), P(.13f, .72f), Color, Stroke);
            Draw.AddLine(P(.87f, .32f), P(.87f, .72f), Color, Stroke);
            Draw.AddLine(P(.5f, .52f), P(.5f, .92f), Color, Stroke);
            Draw.AddLine(P(.13f, .72f), P(.5f, .92f), Color, Stroke);
            Draw.AddLine(P(.87f, .72f), P(.5f, .92f), Color, Stroke);
            break;
        }
        case EIcon::Material:
            Draw.AddCircleFilled(P(.5f, .5f), Size * .37f, IM_COL32(120, 77, 161, 255), 32);
            Draw.AddCircleFilled(P(.43f, .42f), Size * .26f, IM_COL32(175, 119, 224, 255), 32);
            Draw.AddCircleFilled(P(.36f, .33f), Size * .09f, IM_COL32(229, 198, 255, 255), 16);
            Draw.AddCircle(P(.5f, .5f), Size * .37f, IM_COL32(211, 164, 255, 255), 32, Stroke);
            break;
        case EIcon::Texture:
            Draw.AddRect(P(.1f, .18f), P(.9f, .82f), IM_COL32(95, 212, 151, 255), 3, 0, Stroke);
            Draw.AddCircleFilled(P(.72f, .35f), Size * .085f, IM_COL32(239, 210, 116, 255));
            Draw.AddTriangleFilled(P(.16f, .75f), P(.43f, .35f), P(.70f, .75f), IM_COL32(80, 174, 125, 255));
            Draw.AddTriangleFilled(P(.51f, .75f), P(.7f, .5f), P(.84f, .75f), IM_COL32(128, 219, 167, 255));
            break;
        case EIcon::Font:
            Draw.AddText(ImGui::GetFont(), Size * .58f, P(.12f, .16f), IM_COL32(245, 167, 84, 255), "Aa");
            Draw.AddLine(P(.12f, .79f), P(.87f, .79f), IM_COL32(245, 167, 84, 255), Stroke);
            break;
        default:
            Draw.AddRect(P(.22f, .12f), P(.78f, .88f), IM_COL32(155, 155, 160, 255), 3, 0, Stroke);
            Draw.AddText(ImGui::GetFont(), Size * .48f, P(.36f, .24f), IM_COL32(185, 185, 190, 255), "?");
            break;
        }
    }
}

void FContentBrowser::Refresh(const FFileManager& Files, const UAssetManager& Assets)
{
    Initialized = true;
    LastRegistryRevision = Assets.GetRegistryRevision();
    Error.clear();
    Folders.clear();
    Folders[{}];
    try
    {
        Root = Files.GetFileDirectoryPath();
        std::error_code Ec;
        fs::recursive_directory_iterator It(Root, fs::directory_options::skip_permission_denied, Ec), End;
        if (Ec) throw fs::filesystem_error("Cannot browse Assets", Root, Ec);
        for (; It != End; It.increment(Ec))
        {
            if (Ec) break;
            // Do not browse directory links outside the asset root or recurse through link cycles.
            const bool Directory = It->is_directory();
            if (!IsUnder(It->path(), Root) || It->is_symlink())
            {
                if (Directory) It.disable_recursion_pending();
                continue;
            }
            if (!Directory && (!It->is_regular_file() || FName(Utf8(It->path().extension()).c_str()) != FName(".uasset")))
                continue;

            FEntry Entry;
            Entry.Path = It->path().lexically_relative(Root);
            Entry.IsDirectory = Directory;
            Entry.Name = Utf8(Directory ? Entry.Path.filename() : Entry.Path.stem());
            if (Directory) Folders[Entry.Path];
            else
            {
                const FName Name(Utf8(Entry.Path).c_str());
                const auto* Meta = Assets.FindMetaInfo(Name);
                if (!Meta) continue;
                Entry.AssetClass = Meta->AssetClass;
            }
            Folders[Entry.Path.parent_path()].push_back(std::move(Entry));
        }
        if (Ec) Error = Ec.message();
    }
    catch (const std::exception& Exception) { Error = Exception.what(); }

    for (auto& [Path, Entries] : Folders)
        std::sort(Entries.begin(), Entries.end(), [](const FEntry& A, const FEntry& B)
        {
            if (A.IsDirectory != B.IsDirectory) return A.IsDirectory;
            return A.Name < B.Name;
        });
    if (!Folders.contains(CurrentDirectory)) Navigate({});
    if (!SelectedPath.empty())
    {
        const auto Parent = Folders.find(SelectedPath.parent_path());
        if (Parent == Folders.end() || std::none_of(Parent->second.begin(), Parent->second.end(),
            [&](const FEntry& Entry) { return Entry.Path == SelectedPath; })) SelectedPath.clear();
    }
}

void FContentBrowser::Navigate(const fs::path& Path)
{
    NamingFolder.clear(); // An unfinished rename keeps the already-created default folder name.
    CurrentDirectory = Path;
    SelectedPath.clear();
}

void FContentBrowser::DrawFolderTree(const fs::path& Path)
{
    const auto It = Folders.find(Path);
    if (It == Folders.end()) return;
    const bool HasChildren = std::any_of(It->second.begin(), It->second.end(), [](const FEntry& E) { return E.IsDirectory; });
    ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!HasChildren) Flags |= ImGuiTreeNodeFlags_Leaf;
    if (Path.empty()) Flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (Path == CurrentDirectory) Flags |= ImGuiTreeNodeFlags_Selected;
    for (auto Ancestor = CurrentDirectory; !Ancestor.empty(); Ancestor = Ancestor.parent_path())
        if (Ancestor.parent_path() == Path) ImGui::SetNextItemOpen(true);
    const std::string Id = Path.empty() ? "##AssetsRoot" : Utf8(Path);
    const std::string Label = Path.empty() ? "Assets" : Utf8(Path.filename());
    const bool Open = ImGui::TreeNodeEx(Id.c_str(), Flags, "%s", Label.c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) Navigate(Path);
    if (Open)
    {
        for (const auto& Entry : It->second)
            if (Entry.IsDirectory) DrawFolderTree(Entry.Path);
        ImGui::TreePop();
    }
}

void FContentBrowser::Draw(const FFileManager& Files, UAssetManager& Assets, URenderer& Renderer)
{
    ImGui::SetNextWindowSize(ImVec2(800, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Content Browser")) { ImGui::End(); return; }
    if (!Initialized || LastRegistryRevision != Assets.GetRegistryRevision()) Refresh(Files, Assets);
    bool RequestFolder = false, RequestAsset = false, CommitName = false;
    fs::path TogglePath;
    bool ToggleValue = false;

    ImGui::BeginDisabled(CurrentDirectory.empty());
    if (ImGui::Button("Up")) Navigate(CurrentDirectory.parent_path());
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) Refresh(Files, Assets);
    ImGui::SameLine();
    if (ImGui::SmallButton("Assets")) Navigate({});
    fs::path Breadcrumb;
    // Navigate can change CurrentDirectory while drawing a breadcrumb.
    const auto Directory = CurrentDirectory;
    for (const auto& Part : Directory)
    {
        Breadcrumb /= Part;
        ImGui::SameLine(); ImGui::TextUnformatted(">"); ImGui::SameLine();
        ImGui::PushID(Utf8(Breadcrumb).c_str());
        if (ImGui::SmallButton(Utf8(Part).c_str())) Navigate(Breadcrumb);
        ImGui::PopID();
    }
    if (!Error.empty()) ImGui::TextWrapped("%s", Error.c_str());
    ImGui::Separator();

    const float TreeWidth = (std::min)(190.0f, (std::max)(70.0f, ImGui::GetContentRegionAvail().x * .3f));
    if (ImGui::BeginChild("Folders", ImVec2(TreeWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
        DrawFolderTree({});
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("Files", ImVec2(0, 0)))
    {
        const auto It = Folders.find(CurrentDirectory);
        if (It == Folders.end() || It->second.empty()) ImGui::TextDisabled("No folders or .uasset files.");
        else
        {
            const int Columns = (std::clamp)(static_cast<int>(ImGui::GetContentRegionAvail().x / 120.0f), 1, 64);
            if (ImGui::BeginTable("AssetTiles", Columns, ImGuiTableFlags_SizingStretchSame))
            {
                for (const auto& Entry : It->second)
                {
                    const auto* Meta = Entry.IsDirectory ? nullptr : Assets.FindMetaInfo(FName(Utf8(Entry.Path).c_str()));
                    if (!Entry.IsDirectory && !Meta)
                    {
                        // Also hide cached entries removed directly through the manager.
                        if (SelectedPath == Entry.Path) SelectedPath.clear();
                        continue;
                    }
                    ImGui::TableNextColumn();
                    ImGui::PushID(Utf8(Entry.Path).c_str());
                    const float Width = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
                    const float IconSize = (std::min)(70.0f, Width - 8.0f);
                    const float Height = 82.0f + ImGui::GetTextLineHeight() * 3.0f;
                    const ImVec2 Pos = ImGui::GetCursorScreenPos();
                    ImGui::SetNextItemAllowOverlap(); // Inline name/lock controls take precedence over the tile.
                    const bool Clicked = ImGui::InvisibleButton("Tile", ImVec2(Width, Height));
                    const bool Hovered = ImGui::IsItemHovered();
                    if (!Entry.IsDirectory)
                        AssetDragDrop::Source(Utf8(Entry.Path).c_str(), Entry.Name.c_str());
                    if (ImGui::BeginPopupContextItem("Item Actions"))
                    {
                        if (ImGui::MenuItem("Delete"))
                        {
                            DeletePath = Entry.Path;
                            DeleteDirectory = Entry.IsDirectory;
                            SelectedPath = Entry.Path;
                            RequestDeletePopup = true;
                        }
                        ImGui::EndPopup();
                    }
                    const bool Naming = Entry.Path == NamingFolder;
                    const auto& IO = ImGui::GetIO();
                    const bool PlainClick = Clicked && IO.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] <
                        IO.MouseDragThreshold * IO.MouseDragThreshold;
                    if (PlainClick) SelectedPath = Entry.Path;
                    if (Hovered && Entry.IsDirectory && !Naming && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) Navigate(Entry.Path);
                    const EIcon Icon = Entry.IsDirectory ? EIcon::Folder : IconFor(Meta->AssetClass);
                    auto& Draw = *ImGui::GetWindowDrawList();
                    const ImVec2 Max(Pos.x + Width, Pos.y + Height);
                    const bool Selected = SelectedPath == Entry.Path;
                    Draw.AddRectFilled(Pos, Max, ImGui::GetColorU32(Selected ? ImGuiCol_Header : Hovered ? ImGuiCol_HeaderHovered : ImGuiCol_FrameBg), 4);
                    if (Selected) Draw.AddRect(Pos, Max, ImGui::GetColorU32(ImGuiCol_CheckMark), 4);
                    Draw.PushClipRect(Pos, Max, true);
                    if (IconSize > 0) DrawIcon(Draw, Icon, ImVec2(Pos.x + (Width - IconSize) * .5f, Pos.y + 4), IconSize);
                    const float NameY = Pos.y + 78;
                    Draw.PushClipRect(ImVec2(Pos.x, NameY), ImVec2(Max.x, NameY + ImGui::GetTextLineHeight() * 2), true);
                    if (!Naming)
                        Draw.AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(Pos.x + 4, NameY), ImGui::GetColorU32(ImGuiCol_Text),
                            Entry.Name.c_str(), nullptr, (std::max)(1.0f, Width - 8));
                    Draw.PopClipRect();
                    Draw.AddText(ImVec2(Pos.x + 4, Max.y - ImGui::GetTextLineHeight() - 2), ImGui::GetColorU32(ImGuiCol_TextDisabled), TypeLabel(Icon));
                    Draw.PopClipRect();
                    bool ControlHovered = false;
                    if (Meta)
                    {
                        const ImVec2 Lock(Pos.x + Width - 24, Pos.y + 3);
                        ImGui::SetCursorScreenPos(Lock);
                        if (ImGui::InvisibleButton("Standalone", ImVec2(22, 23)))
                        {
                            TogglePath = Entry.Path;
                            ToggleValue = !Meta->bStandalone;
                        }
                        ControlHovered = ImGui::IsItemHovered();
                        const auto Color = ImGui::GetColorU32(Meta->bStandalone ? ImGuiCol_CheckMark : ImGuiCol_TextDisabled);
                        if (ControlHovered) Draw.AddRectFilled(Lock, ImVec2(Lock.x + 22, Lock.y + 23), ImGui::GetColorU32(ImGuiCol_ButtonHovered), 3);
                        Draw.AddRectFilled(ImVec2(Lock.x + 4, Lock.y + 11), ImVec2(Lock.x + 18, Lock.y + 21), Color, 2);
                        const float Shift = Meta->bStandalone ? 0.0f : 5.0f;
                        Draw.AddLine(ImVec2(Lock.x + 7 + Shift, Lock.y + 11), ImVec2(Lock.x + 7 + Shift, Lock.y + 5), Color, 2);
                        Draw.AddLine(ImVec2(Lock.x + 7 + Shift, Lock.y + 5), ImVec2(Lock.x + 15 + Shift, Lock.y + 5), Color, 2);
                        Draw.AddLine(ImVec2(Lock.x + 15 + Shift, Lock.y + 5), ImVec2(Lock.x + 15 + Shift, Lock.y + 11), Color, 2);
                        if (ControlHovered) ImGui::SetTooltip(Meta->bStandalone ?
                            "Standalone: not automatically deleted.\nClick to allow automatic deletion when unreferenced." :
                            "May be automatically deleted when unreferenced.\nClick to keep this asset (Standalone).");
                    }
                    if (Naming)
                    {
                        ImGui::SetCursorScreenPos(ImVec2(Pos.x + 3, NameY));
                        ImGui::SetNextItemWidth((std::max)(1.0f, Width - 6));
                        if (FocusFolderName) { ImGui::SetKeyboardFocusHere(); ImGui::SetScrollHereY(); FocusFolderName = false; }
                        const bool Enter = ImGui::InputText("##FolderName", FolderName, sizeof(FolderName),
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
                        ControlHovered = ControlHovered || ImGui::IsItemHovered();
                        if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape)) NamingFolder.clear();
                        else if (Enter || ImGui::IsItemDeactivatedAfterEdit()) CommitName = true;
                    }
                    ImGui::SetCursorScreenPos(Pos);
                    ImGui::Dummy(ImVec2(Width, Height)); // Restore the tile's layout after overlay controls.
                    if (PlainClick && !Entry.IsDirectory && !ControlHovered) ClickedAsset = Entry.Path;
                    if (Hovered && !ControlHovered)
                    {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(Entry.Name.c_str());
                        ImGui::TextDisabled("%s", TypeLabel(Icon));
                        ImGui::EndTooltip();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        if (ImGui::BeginPopupContextWindow("Content Actions", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            RequestFolder = ImGui::MenuItem("Add Folder");
            RequestAsset = ImGui::MenuItem("Add Asset");
            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();
    // Refresh invalidates tile iterators, so all disk mutations happen after the table/children.
    if (CommitName && !NamingFolder.empty()) CommitFolderName(Files, Assets);
    if (RequestFolder) AddFolder(Files, Assets);
    if (RequestAsset) OpenCreation();
    if (!TogglePath.empty())
    {
        if (Assets.SetAssetStandalone(FName(Utf8(TogglePath).c_str()), ToggleValue)) Refresh(Files, Assets);
        else Error = "Could not save Standalone. See the console for details.";
    }
    DrawCreation(Files, Assets, Renderer);
    DrawDeletion(Files, Assets);
    ImGui::End();
}
