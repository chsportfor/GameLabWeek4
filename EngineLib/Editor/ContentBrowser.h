#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "Rendering/FontResource.h"

class FFileManager;
class UAssetManager;
class URenderer;
struct FClassInfo;

// Browsing reads metadata only; explicit creation actions invoke the importers.
class FContentBrowser
{
public:
    void Draw(const FFileManager& Files, UAssetManager& Assets, URenderer& Renderer);
    const std::filesystem::path& GetCurrentFolder() const { return CurrentDirectory; }
    const std::filesystem::path& GetSelectedPath() const { return SelectedPath; }
    std::filesystem::path ConsumeAssetClick() { auto Path = ClickedAsset; ClickedAsset.clear(); return Path; }
    std::optional<std::filesystem::path> ConsumeStaticMeshImport()
    {
        auto Folder = StaticMeshImportFolder;
        StaticMeshImportFolder.reset();
        return Folder;
    }

private:
    struct FEntry
    {
        std::filesystem::path Path; // Relative to Assets, including extension; also the UI identity.
        std::string Name;
        bool IsDirectory = false;
        const FClassInfo* AssetClass = nullptr;
    };

    void Refresh(const FFileManager& Files, const UAssetManager& Assets);
    void DrawFolderTree(const std::filesystem::path& Path);
    void Navigate(const std::filesystem::path& Path);
    void AddFolder(const FFileManager& Files, const UAssetManager& Assets);
    void CommitFolderName(const FFileManager& Files, const UAssetManager& Assets);
    void OpenCreation();
    void DrawCreation(const FFileManager& Files, UAssetManager& Assets, URenderer& Renderer);
    bool CreateAsset(const FFileManager& Files, UAssetManager& Assets, URenderer& Renderer);
    void DrawDeletion(const FFileManager& Files, UAssetManager& Assets);
    bool DeleteSelected(UAssetManager& Assets);

    std::filesystem::path DeletePath;
    bool DeleteDirectory = false;
    bool RequestDeletePopup = false;
    std::string DeleteError;
    std::vector<std::string> DeleteBlockedPaths;

    std::filesystem::path NamingFolder;
    char FolderName[256]{};
    bool FocusFolderName = false;
    enum class ECreateType { Texture, Material, Font };
    std::optional<std::filesystem::path> StaticMeshImportFolder;
    ECreateType CreateType = ECreateType::Texture;
    int MaterialMode = 0; // MTL / image / existing Texture2D
    int FontMode = 0; // MSDF / bitmap grid
    char SourcePath[4096]{};
    char MetadataPath[4096]{};
    char AssetName[256]{};
    std::filesystem::path CreationDirectory;
    std::string TexturePath;
    std::filesystem::path TextureBrowseDirectory;
    std::string TextureBrowseSelection;
    float DiffuseColor[4]{1, 1, 1, 1};
    FBitmapFontAtlasSettings BitmapSettings;
    std::string CreateError;

    std::filesystem::path Root;
    std::filesystem::path CurrentDirectory;
    std::filesystem::path SelectedPath;
    std::filesystem::path ClickedAsset;
    std::map<std::filesystem::path, std::vector<FEntry>> Folders;
    std::string Error;
    bool Initialized = false;
    uint64 LastRegistryRevision = 0;
};
