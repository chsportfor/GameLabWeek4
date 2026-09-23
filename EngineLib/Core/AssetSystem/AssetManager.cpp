#include "AssetManager.h"
#include "Core/IO/FileManager.h"
#include "Core/AssetSystem/AssetFile/AssetFile.h"
#include "Core/AssetSystem/AssetFile/AssetFileSchema.h"
#include "Editor/Console.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>

IMPLEMENT_CLASS(UAssetManager, UObject);

namespace
{
    namespace fs = std::filesystem;
    bool IsAssetClass(const FClassInfo* Class)
    {
        for (auto* Current = Class; Current; Current = Current->SuperClass)
            if (Current == UAsset::GetClass()) return true;
        return false;
    }
    FName PathName(const fs::path& Path)
    {
        const auto Text = Path.generic_u8string();
        return FName(FString(std::string_view(reinterpret_cast<const char*>(Text.data()), Text.size())));
    }
    fs::path AssetPath(const fs::path& Root, const FName& Name)
    {
        const auto Relative = fs::u8path(Name.ToString().CStr());
        const auto Path = fs::weakly_canonical(Root / Relative);
        const auto Checked = Path.lexically_relative(Root);
        if (Relative.is_absolute() || Checked.empty() || Checked.is_absolute() || *Checked.begin() == "..")
            throw std::invalid_argument("Asset path escapes the asset root");
        return Path;
    }

    void RemoveEmptyAssetDirectories(const fs::path& DeletedFile, const fs::path& Root)
    {
        // DeletedFile is already resolved by AssetPath. Never remove the asset root or
        // recursively erase contents: RemoveDirectoryW succeeds only for an empty directory.
        for (auto Directory = DeletedFile.parent_path(); !Directory.empty(); Directory = Directory.parent_path())
        {
            const auto Relative = Directory.lexically_relative(Root);
            if (Relative.empty() || Relative == "." || Relative.is_absolute() || *Relative.begin() == "..") return;
            if (RemoveDirectoryW(Directory.c_str())) continue;
            const DWORD Error = GetLastError();
            if (Error == ERROR_DIR_NOT_EMPTY) return;
            // Another deleted asset in the same batch may already have pruned this path.
            if (Error == ERROR_PATH_NOT_FOUND || Error == ERROR_FILE_NOT_FOUND) continue;
            UE_LOG(Warning, Core, "Asset deleted, but empty-folder cleanup failed (%s, Win32 %lu).",
                Directory.string().c_str(), Error);
            return; // Cleanup failure does not undo or fail a successful asset deletion.
        }
    }
}

void UAssetManager::Initialize(URenderer& InRenderer)
{
    Clear();
    Renderer = &InRenderer;
    AssetRoot = FFileManager::Get().GetFileDirectoryPath();
}

FName UAssetManager::NormalizeAssetName(const FName& Name)
{
    if (!Name.IsValid()) return {};
    std::string Text = Name.ToString().CStr();
    std::replace(Text.begin(), Text.end(), '\\', '/');
    const auto Path = fs::u8path(Text).lexically_normal();
    if (Path.empty() || Path.is_absolute() || Path.has_root_name() || *Path.begin() == ".." ||
        PathName(Path.extension()) != FName(".uasset"))
        throw std::invalid_argument("Asset name must be a relative .uasset path");
    return PathName(Path);
}

FName UAssetManager::MakeFileAssetName(const fs::path& Path, const FFileManager& Files)
{
    const auto Root = Files.GetFileDirectoryPath();
    const auto Relative = Files.ResolvePath(Path).lexically_relative(Root);
    return NormalizeAssetName(PathName(Relative));
}

FAssetMetaInfo UAssetManager::ReadMetaInfo(const fs::path& Path, bool UpgradeFile) const
{
    if (AssetRoot.empty()) throw std::logic_error("Initialize the asset manager before registration");
    const auto Name = NormalizeAssetName(PathName(Path.is_absolute() ?
        fs::weakly_canonical(Path).lexically_relative(AssetRoot) : Path));
    const auto FilePath = AssetPath(AssetRoot, Name);
    std::ifstream Stream(FilePath, std::ios::binary);
    auto Header = AssetFile::ReadHeader(Stream);
    Stream.close(); // Release the file before a possible replacement.
    const auto* Class = FObjectFactory::GetClassInfoByName(Header.AssetType);
    if (!Class || !Class->Constructor || !IsAssetClass(Class))
        throw std::runtime_error("Asset header must name a concrete UAsset class");
    const auto& Schema = AssetFile::GetSchema(Class);
    if (UpgradeFile)
    {
        const auto OldVersion = Header.SchemaVersion;
        Header = AssetFile::UpgradeFile(FilePath, Header, Schema);
        if (OldVersion != Header.SchemaVersion)
            UE_LOG(Log, Core, "Upgraded asset %s: schema %u -> %u", Name.ToString().CStr(), OldVersion, Header.SchemaVersion);
    }
    else if (Header.SchemaVersion != Schema.LatestVersion)
        throw std::runtime_error("Asset schema changed; scan/register it before loading");
    FAssetMetaInfo Meta{Name, Class, Header.bStandalone, {}};
    std::unordered_set<FName> Unique;
    for (const auto& Dependency : Header.Dependencies)
    {
        const auto Key = NormalizeAssetName(FName(Dependency));
        if (!Key.IsValid()) throw std::runtime_error("Empty asset dependency");
        AssetPath(AssetRoot, Key); // Reject paths escaping through directory links too.
        if (Unique.insert(Key).second) Meta.Dependencies.Add(Key);
    }
    return Meta;
}

bool UAssetManager::RegisterAsset(const fs::path& Path)
{
    try
    {
        const auto Meta = ReadMetaInfo(Path, true);
        if (const auto* Existing = AssetMetaInfoMap.Find(Meta.AssetName))
        {
            if (Existing->AssetClass != Meta.AssetClass)
                throw std::runtime_error("Registered asset path already has another class");
            // Refresh disk metadata; already loaded objects keep their current resources.
            for (const auto& Dependency : Existing->Dependencies)
                if (auto* References = ReverseReferences.Find(Dependency))
                {
                    References->erase(Meta.AssetName);
                    if (References->empty()) ReverseReferences.Remove(Dependency);
                }
        }
        else UAsset::GetNameRegistry(Meta.AssetClass).Add(Meta.AssetName);
        AssetMetaInfoMap.Add(Meta.AssetName, Meta);
        // The dependency's name is enough; its own metadata may be registered later.
        // Do not reset ReverseReferences[Meta.AssetName]: earlier registrations may refer to it.
        for (const auto& Dependency : Meta.Dependencies)
            ReverseReferences[Dependency].insert(Meta.AssetName);
        ++RegistryRevision;
        return true;
    }
    catch (const std::exception& Error)
    {
        UE_DEBUG_LOG_ERROR(Core, "Asset registration failed (%s): %s", Path.string().c_str(), Error.what());
        return false;
    }
}

bool UAssetManager::ScanAssets()
{
    try
    {
        if (AssetRoot.empty()) throw std::logic_error("Initialize the asset manager before scanning");
        TArray<fs::path> Paths;
        for (const auto& Entry : fs::recursive_directory_iterator(AssetRoot))
            if (Entry.is_regular_file() && PathName(Entry.path().extension()) == FName(".uasset")) Paths.Add(Entry.path());
        TMap<FName, FAssetMetaInfo> Scanned;
        TMap<FName, std::unordered_set<FName>> ScannedReferences;
        for (const auto& Path : Paths)
        {
            const auto Meta = ReadMetaInfo(Path, true);
            if (Scanned.Contains(Meta.AssetName)) throw std::runtime_error("Duplicate case-insensitive asset name");
            Scanned.Add(Meta.AssetName, Meta);
            for (const auto& Dependency : Meta.Dependencies)
                ScannedReferences[Dependency].insert(Meta.AssetName);
        }
        for (const auto& [Name, Meta] : AssetMetaInfoMap)
        {
            UAsset::GetNameRegistry(Meta.AssetClass).Remove(Name);
            const auto* NewMeta = Scanned.Find(Name);
            if (!NewMeta || NewMeta->AssetClass != Meta.AssetClass) RetireLoadedAsset(Name);
        }
        AssetMetaInfoMap = std::move(Scanned);
        ReverseReferences = std::move(ScannedReferences);
        for (const auto& [Name, Meta] : AssetMetaInfoMap) UAsset::GetNameRegistry(Meta.AssetClass).Add(Name);
        ++RegistryRevision;
        return true;
    }
    catch (const std::exception& Error)
    {
        UE_DEBUG_LOG_ERROR(Core, "Asset header scan failed; previous index retained: %s", Error.what());
        return false;
    }
}

bool UAssetManager::SetAssetStandalone(const FName& Name, bool Standalone)
{
    try
    {
        const auto Key = NormalizeAssetName(Name);
        const auto Path = AssetPath(AssetRoot, Key);
        if (!AssetMetaInfoMap.Contains(Key) && !RegisterAsset(Path)) return false;
        const auto DiskMeta = ReadMetaInfo(Path);
        auto* Meta = AssetMetaInfoMap.Find(Key);
        if (DiskMeta.AssetClass != Meta->AssetClass)
            throw std::runtime_error("Asset class changed; refresh registration first");
        AssetFile::SetStandalone(Path, Standalone);
        // External dependency edits are reflected by RegisterAsset/ScanAssets, not this toggle.
        Meta->bStandalone = Standalone;
        return true;
    }
    catch (const std::exception& Error)
    {
        UE_LOG(Error, Core, "Cannot update asset Standalone (%s): %s", Name.ToString().CStr(), Error.what());
        return false;
    }
}

const FAssetMetaInfo* UAssetManager::FindMetaInfo(const FName& Name) const
{
    return AssetMetaInfoMap.Find(NormalizeAssetName(Name));
}

UAsset* UAssetManager::LoadAsset(const FName& Name)
{
    const auto Key = NormalizeAssetName(Name);
    if (const auto* Loaded = LoadedAssets.Find(Key)) return *Loaded;
    const auto* Entry = AssetMetaInfoMap.Find(Key);
    if (!Entry) return nullptr;
    const auto Meta = *Entry;
    const auto Path = AssetPath(AssetRoot, Key);
    if (!fs::exists(Path)) return nullptr;
    if (!Renderer) throw std::logic_error("Asset loading requires an initialized manager");
    if (!LoadingAssets.insert(Key).second) throw std::runtime_error("Cyclic asset load dependency");
    try
    {
        // Detect replacement after registration rather than dispatching the wrong body parser.
        if (ReadMetaInfo(Path).AssetClass != Meta.AssetClass)
            throw std::runtime_error("Asset class changed after registration");
        std::unique_ptr<UAsset> Asset(static_cast<UAsset*>(
            FObjectFactory::ConstructUnInitializedObject(Meta.AssetClass)));
        if (!Asset) throw std::runtime_error("Asset construction failed");
        Asset->SetName(Key);
        Asset->Load(Path, *this, *Renderer);
        LoadedAssets.Add(Key, Asset.get());
        LoadingAssets.erase(Key);
        return Asset.release();
    }
    catch (const std::exception& Error)
    {
        LoadingAssets.erase(Key);
        UE_DEBUG_LOG_ERROR(Core, "Asset load failed (%s): %s", Key.ToString().CStr(), Error.what());
        throw;
    }
    catch (...) { LoadingAssets.erase(Key); throw; }
}

UAsset* UAssetManager::GetAsset(const FName& Name, bool LoadIfNotLoaded)
{
    const auto Key = NormalizeAssetName(Name);
    if (const auto* Loaded = LoadedAssets.Find(Key)) return *Loaded;
    return LoadIfNotLoaded ? LoadAsset(Key) : nullptr;
}

TArray<FName> UAssetManager::GetReferencers(const FName& Name) const
{
    TArray<FName> Result;
    if (const auto* References = ReverseReferences.Find(NormalizeAssetName(Name)))
        for (const auto& Reference : *References) Result.Add(Reference);
    return Result;
}

void UAssetManager::RetireLoadedAsset(const FName& Name)
{
    if (auto* Asset = LoadedAssets.Find(Name))
    {
        RetiredAssets.Add(*Asset);
        LoadedAssets.Remove(Name);
    }
}

UAsset* UAssetManager::LoadDefaultAsset(const FName& MissingName, const FClassInfo* ExpectedClass, const FName& DefaultName)
{
    const auto Key = NormalizeAssetName(DefaultName);
    // Never retry a missing default through the fallback path.
    if (!Key.IsValid() || Key == MissingName) return nullptr;
    UE_LOG_WARN(Core, "Missing asset %s; using %s", MissingName.ToString().CStr(), Key.ToString().CStr());
    if (auto* Asset = GetAsset(Key))
        return Asset->GetRuntimeClass() == ExpectedClass ? Asset : nullptr;
    const auto* Meta = FindMetaInfo(Key);
    if (!Meta || Meta->AssetClass != ExpectedClass) return nullptr;
    return LoadAsset(Key);
}

bool UAssetManager::DeleteUnreferenced(const FName& Name)
{
    const auto* Entry = AssetMetaInfoMap.Find(Name);
    if (!Entry) return true;
    const auto* Referencers = ReverseReferences.Find(Name);
    if ((Referencers && !Referencers->empty()) || Entry->bStandalone) return true;
    const auto Meta = *Entry;
    // Commit this graph change only after the file is removed successfully.
    const auto Path = AssetPath(AssetRoot, Name);
    if (!fs::remove(Path))
        throw std::runtime_error("Asset file disappeared during deletion");
    ForgetDeletedAsset(Meta);
    RemoveEmptyAssetDirectories(Path, AssetRoot);
    // No permanent visited mark for retained dependencies: shared leaves must be reconsidered.
    bool Success = true;
    for (const auto& Dependency : Meta.Dependencies)
    {
        try { if (!DeleteUnreferenced(Dependency)) Success = false; }
        catch (const std::exception& Error)
        {
            UE_DEBUG_LOG_ERROR(Core, "Dependency file deletion failed (%s): %s", Dependency.ToString().CStr(), Error.what());
            Success = false;
        }
    }
    return Success;
}

bool UAssetManager::DeleteAsset(const FName& Name)
{
    return DeleteAssets({Name});
}

void UAssetManager::ForgetDeletedAsset(const FAssetMetaInfo& Meta)
{
    ++RegistryRevision;
    RetireLoadedAsset(Meta.AssetName);
    UAsset::GetNameRegistry(Meta.AssetClass).Remove(Meta.AssetName);
    AssetMetaInfoMap.Remove(Meta.AssetName);
    ReverseReferences.Remove(Meta.AssetName);
    for (const auto& Dependency : Meta.Dependencies)
        if (auto* References = ReverseReferences.Find(Dependency))
        {
            References->erase(Meta.AssetName);
            if (References->empty()) ReverseReferences.Remove(Dependency);
        }
}

TArray<FName> UAssetManager::GetDeletionBlockers(const TArray<FName>& Names) const
{
    std::unordered_set<FName> Targets, Blocked;
    for (const auto& Name : Names) Targets.insert(NormalizeAssetName(Name));
    TArray<FName> Result;
    for (const auto& Name : Targets)
        if (const auto* References = ReverseReferences.Find(Name))
            for (const auto& Reference : *References)
                if (!Targets.contains(Reference))
                {
                    Blocked.insert(Name); Result.Add(Name); break;
                }
    // If an externally retained target needs another target, that target must also survive.
    // The visited set handles diamonds/cycles; no dependency ordering is necessary.
    for (int32 I = 0; I < Result.Num(); ++I)
        if (const auto* Meta = AssetMetaInfoMap.Find(Result[I]))
            for (const auto& Dependency : Meta->Dependencies)
                if (Targets.contains(Dependency) && Blocked.insert(Dependency).second) Result.Add(Dependency);
    return Result;
}

bool UAssetManager::DeleteAssets(const TArray<FName>& Names)
{
    try
    {
        if (!GetDeletionBlockers(Names).IsEmpty())
            throw std::runtime_error("Assets outside the deletion set still reference its contents");
        struct FTarget { FAssetMetaInfo Meta; fs::path Path; fs::path Staged; };
        std::vector<FTarget> Targets;
        std::unordered_set<FName> Seen;
        for (const auto& Name : Names)
        {
            const auto Key = NormalizeAssetName(Name);
            if (!Seen.insert(Key).second) continue;
            const auto* Meta = AssetMetaInfoMap.Find(Key);
            if (!Meta) throw std::runtime_error("Cannot delete an unregistered asset");
            const auto Path = AssetPath(AssetRoot, Key);
            auto Staged = Path; Staged += L".deleting";
            if (!fs::is_regular_file(Path) || fs::exists(Staged))
                throw std::runtime_error("Missing asset or unfinished .deleting file: " + Path.string());
            if (GetFileAttributesW(Path.c_str()) & FILE_ATTRIBUTE_READONLY)
                throw std::runtime_error("Cannot delete read-only asset: " + Path.string());
            Targets.push_back({*Meta, Path, Staged});
        }
        // Move every explicit target first. A locked file aborts before deleting any bytes.
        // This also permits cycles wholly inside the deletion set without an arbitrary order.
        size_t StagedCount = 0;
        try
        {
            for (const auto& Target : Targets)
            {
                if (!MoveFileExW(Target.Path.c_str(), Target.Staged.c_str(), MOVEFILE_WRITE_THROUGH))
                    throw std::system_error(int(GetLastError()), std::system_category(), "Stage asset deletion");
                ++StagedCount;
            }
        }
        catch (...)
        {
            while (StagedCount)
            {
                const auto& Target = Targets[--StagedCount];
                if (!MoveFileExW(Target.Staged.c_str(), Target.Path.c_str(), MOVEFILE_WRITE_THROUGH))
                    UE_LOG(Error, Core, "Deletion rollback failed; recover %s to %s", Target.Staged.string().c_str(), Target.Path.string().c_str());
            }
            throw;
        }
        for (const auto& Target : Targets) ForgetDeletedAsset(Target.Meta);
        bool Success = true;
        for (const auto& Target : Targets)
        {
            std::error_code Error;
            if (!fs::remove(Target.Staged, Error))
            {
                UE_LOG(Error, Core, "Cannot remove staged asset %s: %s", Target.Staged.string().c_str(), Error.message().c_str());
                Success = false;
            }
            else RemoveEmptyAssetDirectories(Target.Path, AssetRoot);
        }
        for (const auto& Target : Targets)
            for (const auto& Dependency : Target.Meta.Dependencies)
                try { if (!DeleteUnreferenced(Dependency)) Success = false; }
                catch (const std::exception& Error)
                {
                    UE_LOG(Error, Core, "Dependency deletion failed (%s): %s", Dependency.ToString().CStr(), Error.what());
                    Success = false;
                }
        return Success;
    }
    catch (const std::exception& Error)
    {
        UE_DEBUG_LOG_ERROR(Core, "Asset file deletion failed: %s", Error.what());
        return false;
    }
}

void UAssetManager::Clear()
{
    ++RegistryRevision;
    for (const auto& [Name, Asset] : LoadedAssets) delete Asset;
    LoadedAssets.Empty();
    for (auto* Asset : RetiredAssets) delete Asset;
    RetiredAssets.Empty();
    for (const auto& [Name, Meta] : AssetMetaInfoMap) UAsset::GetNameRegistry(Meta.AssetClass).Remove(Name);
    AssetMetaInfoMap.Empty();
    ReverseReferences.Empty();
    LoadingAssets.clear();
    Renderer = nullptr;
    AssetRoot.clear();
}

UObject* LoadAssetReference(const FName& Name, const FClassInfo* ExpectedClass, const FName& DefaultName)
{
    auto* Manager = FObjectFactory::GetDefaultAssetManager();
    if (!Manager) throw std::runtime_error("Asset reference requires an asset manager");
    auto* Asset = Manager->GetAsset(Name);
    if (!Asset)
    {
        if (const auto* Meta = Manager->FindMetaInfo(Name))
        {
            auto* Class = Meta->AssetClass;
            while (Class && Class != ExpectedClass) Class = Class->SuperClass;
            if (!Class) throw std::runtime_error("Incompatible asset reference class");
        }
        Asset = Manager->GetAsset(Name, true);
    }
    if (!Asset)
    {
        UE_LOG_WARN(Core, "Missing asset reference %s; using %s", Name.ToString().CStr(), DefaultName.ToString().CStr());
        Asset = DefaultName.IsValid() ? Manager->GetAsset(DefaultName, true) : nullptr;
    }
    if (!Asset || !Asset->IsA(ExpectedClass))
        throw std::runtime_error(std::string("Missing default or incompatible asset reference: ") + Name.ToString().CStr());
    return Asset;
}
