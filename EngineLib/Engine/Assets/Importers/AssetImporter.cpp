#include "AssetImporter.h"
#include "Core/IO/FileManager.h"
#include "Editor/Console.h"
#include <fstream>
#include <stdexcept>

namespace
{
    namespace fs = std::filesystem;

    void Require(bool Success, const char* Operation)
    {
        if (!Success) throw std::runtime_error(std::string(Operation) +
            " (Win32 " + std::to_string(GetLastError()) + ")");
    }

    struct FTemporaryFile
    {
        fs::path Path;
        explicit FTemporaryFile(const fs::path& Directory)
        {
            wchar_t Buffer[MAX_PATH]{};
            Require(GetTempFileNameW(Directory.c_str(), L"uai", 0, Buffer) != 0, "Create import temporary file");
            Path = Buffer;
        }
        ~FTemporaryFile()
        {
            std::error_code Error;
            if (!Path.empty()) fs::remove(Path, Error);
            if (Error) UE_DEBUG_LOG_ERROR(Core, "Import cleanup failed: %s", Error.message().c_str());
        }
    };

    struct FCreatedDirectories
    {
        std::vector<fs::path> Paths;
        bool Committed = false;
        void Create(const fs::path& Directory)
        {
            std::vector<fs::path> Missing;
            for (auto Path = Directory; !Path.empty() && !fs::exists(Path); Path = Path.parent_path())
                Missing.push_back(Path);
            for (auto It = Missing.rbegin(); It != Missing.rend(); ++It)
            {
                Paths.push_back(*It);
                if (!fs::create_directory(*It)) Paths.pop_back();
            }
        }
        ~FCreatedDirectories()
        {
            if (Committed) return;
            for (auto It = Paths.rbegin(); It != Paths.rend(); ++It)
            {
                std::error_code Error;
                fs::remove(*It, Error); // Nonrecursive: never removes someone else's contents.
                if (Error) UE_DEBUG_LOG_ERROR(Core, "Import directory cleanup failed: %s", Error.message().c_str());
            }
        }
    };

    fs::path ResolveAssetPath(const fs::path& Requested)
    {
        const auto Root = FFileManager::Get().GetFileDirectoryPath();
        const auto Path = FFileManager::Get().ResolvePath(Requested);
        const auto Relative = fs::relative(Path, Root);
        if (Relative.empty() || *Relative.begin() == ".." || Relative.is_absolute() || Path.extension() != ".uasset")
            throw std::runtime_error("Import destination must be a .uasset file inside the asset root");
        return Path;
    }

    void WriteFile(const fs::path& Path, std::span<const uint8> Bytes)
    {
        std::ofstream Out;
        Out.exceptions(std::ios::failbit | std::ios::badbit);
        Out.open(Path, std::ios::binary | std::ios::trunc);
        Out.write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
        Out.close();
    }
}

TArray<FName> FAssetImporter::WriteImportedAsset(const std::filesystem::path& AssetPath, std::span<const uint8> Bytes)
{
    const FAssetFileToWrite File{AssetPath, Bytes};
    return WriteImportedAssets({&File, 1});
}

TArray<FName> FAssetImporter::WriteImportedAssets(std::span<const FAssetFileToWrite> Files)
{
    FCreatedDirectories Directories;
    std::vector<fs::path> CreatedAssets;
    try
    {
        if (Files.empty()) throw std::runtime_error("No asset files to import");
        TArray<FName> Names;
        Names.Reserve(static_cast<uint32>(Files.size()));
        std::vector<fs::path> Paths;
        Paths.reserve(Files.size());
        CreatedAssets.reserve(Files.size());
        for (const auto& File : Files)
        {
            const auto Path = ResolveAssetPath(File.Path);
            if (File.Bytes.empty()) throw std::runtime_error("Cannot import empty asset data");
            if (fs::exists(Path)) throw std::runtime_error("Asset destination already exists: " + Path.string());
            for (const auto& Existing : Paths)
                if (CompareStringOrdinal(Path.c_str(), -1, Existing.c_str(), -1, TRUE) == CSTR_EQUAL)
                    throw std::runtime_error("Duplicate output paths in import batch");
            Paths.push_back(Path);
            const auto Relative = fs::relative(Path, FFileManager::Get().GetFileDirectoryPath()).generic_u8string();
            Names.Add(FName(reinterpret_cast<const char*>(Relative.c_str())));
        }
        for (size_t I = 0; I < Files.size(); ++I)
        {
            const auto& Path = Paths[I];
            Directories.Create(Path.parent_path());
            FTemporaryFile Temporary(Path.parent_path());
            WriteFile(Temporary.Path, Files[I].Bytes);
            CreatedAssets.push_back(Path);
            if (!MoveFileExW(Temporary.Path.c_str(), Path.c_str(), MOVEFILE_WRITE_THROUGH))
            {
                CreatedAssets.pop_back(); // This invocation does not own the destination.
                Require(false, "Commit imported asset");
            }
            Temporary.Path.clear();
        }
        Directories.Committed = true;
        return Names;
    }
    catch (const std::exception& Error)
    {
        for (auto It = CreatedAssets.rbegin(); It != CreatedAssets.rend(); ++It)
        {
            std::error_code CleanupError;
            fs::remove(*It, CleanupError);
            if (CleanupError) UE_DEBUG_LOG_ERROR(Core, "Asset rollback failed: %s", CleanupError.message().c_str());
        }
        UE_DEBUG_LOG_ERROR(Core, "Asset file import failed: %s", Error.what());
        return {};
    }
}
