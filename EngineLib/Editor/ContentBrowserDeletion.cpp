#include "ContentBrowser.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/IO/FileManager.h"
#include "Editor/Console.h"
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
}

bool FContentBrowser::DeleteSelected(UAssetManager& Assets)
{
    DeleteError.clear(); DeleteBlockedPaths.clear();
    try
    {
        const auto Target = fs::weakly_canonical(Root / DeletePath);
        if (DeletePath.empty() || Target == fs::weakly_canonical(Root) || !IsUnder(Target, Root))
            throw std::runtime_error("The Assets root cannot be deleted. Choose an item inside Assets.");
        if (fs::is_directory(Target) != DeleteDirectory)
            throw std::runtime_error("The selected item's type changed. Refresh the browser first.");

        TArray<FName> Names;
        std::vector<fs::path> OtherFiles, Directories;
        auto AddAsset = [&](const fs::path& Path)
        {
            const auto Relative = Path.lexically_relative(Root);
            const FName Name(Utf8(Relative).c_str());
            // Newly discovered local files must enter the registry before graph deletion.
            // This is not a global rescan; external changes elsewhere require an explicit scan.
            if (!Assets.FindMetaInfo(Name) && !Assets.RegisterAsset(Path))
                throw std::runtime_error("Cannot register asset for deletion: " + Utf8(Relative));
            Names.Add(Name);
        };
        if (!DeleteDirectory) AddAsset(Target);
        else
        {
            Directories.push_back(Target);
            for (const auto& Entry : fs::recursive_directory_iterator(Target))
            {
                if (Entry.is_symlink() || !IsUnder(Entry.path(), Target))
                    throw std::runtime_error("Folder contains a link. Remove the link separately before deleting this folder.");
                if (Entry.is_directory()) Directories.push_back(Entry.path());
                else if (!Entry.is_regular_file()) throw std::runtime_error("Folder contains an unsupported file type.");
                else if (FName(Utf8(Entry.path().extension()).c_str()) == FName(".uasset")) AddAsset(Entry.path());
                else OtherFiles.push_back(Entry.path());
            }
        }
        const auto Blocked = Assets.GetDeletionBlockers(Names);
        for (const auto& Name : Blocked) DeleteBlockedPaths.emplace_back(Name.ToString().CStr());
        std::sort(DeleteBlockedPaths.begin(), DeleteBlockedPaths.end());
        if (!DeleteBlockedPaths.empty()) return false; // No deletion has started.
        if (!Assets.DeleteAssets(Names))
            throw std::runtime_error("File deletion failed. Some files may already have been deleted. See the console for details.");
        // Remove only the enumerated files. Never recursively erase unexpected new files.
        for (const auto& Path : OtherFiles)
        {
            if (!IsUnder(Path, Target)) throw std::runtime_error("Folder contents changed during deletion.");
            fs::remove(Path);
        }
        for (auto It = Directories.rbegin(); It != Directories.rend(); ++It)
        {
            if (!IsUnder(*It, Target)) throw std::runtime_error("Folder contents changed during deletion.");
            fs::remove(*It); // Fails rather than deleting any new/unexpected contents.
        }
        return true;
    }
    catch (const std::exception& Ex)
    {
        DeleteError = Ex.what();
        UE_LOG(Error, Editor, "Content Browser deletion failed (%s): %s", Utf8(DeletePath).c_str(), Ex.what());
        return false;
    }
}

void FContentBrowser::DrawDeletion(const FFileManager& Files, UAssetManager& Assets)
{
    if (RequestDeletePopup)
    {
        RequestDeletePopup = false;
        DeleteError.clear(); DeleteBlockedPaths.clear();
        NamingFolder.clear();
        ImGui::OpenPopup("Confirm Delete");
    }
    bool ShowFailure = false;
    bool Open = true; // Title-bar X closes the popup without executing the action.
    ImGui::SetNextWindowSize(ImVec2(600, 360), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(450, 300), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::BeginPopupModal("Confirm Delete", &Open))
    {
        ImGui::BeginChild("Warning", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 12));
        ImGui::TextWrapped("Permanently delete %s?", DeleteDirectory ? "this folder and its contents" : "this asset");
        ImGui::Spacing();
        ImGui::TextWrapped("Assets/%s", Utf8(DeletePath).c_str());
        ImGui::Spacing();
        ImGui::TextWrapped("Unreferenced non-Standalone dependencies may also be deleted, including assets in other folders. Folders left empty by asset deletion are removed up to, but not including, Assets.");
        ImGui::EndChild(); ImGui::Separator();
        ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - 190));
        if (ImGui::Button("Delete", ImVec2(80, 0)))
        {
            ShowFailure = !DeleteSelected(Assets);
            Refresh(Files, Assets);
            if (!ShowFailure) SelectedPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (ShowFailure) ImGui::OpenPopup("Deletion Failed");
    ImGui::SetNextWindowSize(ImVec2(650, 370), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(450, 250), ImVec2(FLT_MAX, FLT_MAX));
    Open = true;
    if (ImGui::BeginPopupModal("Deletion Failed", &Open))
    {
        ImGui::BeginChild("Failure Details", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 12));
        if (!DeleteBlockedPaths.empty())
        {
            ImGui::TextWrapped(DeleteDirectory ? "Deletion Failed - These assets are referenced by assets outside this folder :" :
                "This asset is referenced by other assets :");
            ImGui::Separator();
            for (const auto& Path : DeleteBlockedPaths) ImGui::TextWrapped("Assets/%s", Path.c_str());
        }
        else ImGui::TextWrapped("%s", DeleteError.c_str());
        ImGui::EndChild(); ImGui::Separator();
        ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - 100));
        if (ImGui::Button("Close", ImVec2(80, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
