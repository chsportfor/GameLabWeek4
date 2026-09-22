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
        }
        else UAsset::GetNameRegistry(Meta.AssetClass).Add(Meta.AssetName);
        AssetMetaInfoMap.Add(Meta.AssetName, Meta);
        RebuildReverseReferences();
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
        for (const auto& Path : Paths)
        {
            const auto Meta = ReadMetaInfo(Path, true);
            if (Scanned.Contains(Meta.AssetName)) throw std::runtime_error("Duplicate case-insensitive asset name");
            Scanned.Add(Meta.AssetName, Meta);
        }
        for (const auto& [Name, Meta] : AssetMetaInfoMap)
        {
            UAsset::GetNameRegistry(Meta.AssetClass).Remove(Name);
            const auto* NewMeta = Scanned.Find(Name);
            if (!NewMeta || NewMeta->AssetClass != Meta.AssetClass) RetireLoadedAsset(Name);
        }
        AssetMetaInfoMap = std::move(Scanned);
        for (const auto& [Name, Meta] : AssetMetaInfoMap) UAsset::GetNameRegistry(Meta.AssetClass).Add(Name);
        RebuildReverseReferences();
        return true;
    }
    catch (const std::exception& Error)
    {
        UE_DEBUG_LOG_ERROR(Core, "Asset header scan failed; previous index retained: %s", Error.what());
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

void UAssetManager::RebuildReverseReferences()
{
    ReverseReferences.Empty();
    for (const auto& [Name, Meta] : AssetMetaInfoMap)
        for (const auto& Dependency : Meta.Dependencies) ReverseReferences[Dependency].insert(Name);
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

bool UAssetManager::DeleteUnreferenced(const FName& Name, bool DirectTarget)
{
    const auto* Entry = AssetMetaInfoMap.Find(Name);
    if (!Entry) return !DirectTarget;
    const auto* Referencers = ReverseReferences.Find(Name);
    if ((Referencers && !Referencers->empty()) || (!DirectTarget && Entry->bStandalone))
    {
        if (DirectTarget) UE_LOG_ERROR(Core, "Cannot delete referenced asset: %s", Name.ToString().CStr());
        return !DirectTarget;
    }
    const auto Meta = *Entry;
    // Commit this graph change only after the file is removed successfully.
    if (!fs::remove(AssetPath(AssetRoot, Name)))
        throw std::runtime_error("Asset file disappeared during deletion");
    RetireLoadedAsset(Name);
    UAsset::GetNameRegistry(Meta.AssetClass).Remove(Name);
    AssetMetaInfoMap.Remove(Name);
    ReverseReferences.Remove(Name);
    for (const auto& Dependency : Meta.Dependencies)
        if (auto* References = ReverseReferences.Find(Dependency)) References->erase(Name);
    // No permanent visited mark for retained dependencies: shared leaves must be reconsidered.
    bool Success = true;
    for (const auto& Dependency : Meta.Dependencies)
    {
        try { if (!DeleteUnreferenced(Dependency, false)) Success = false; }
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
    try
    {
        return DeleteUnreferenced(NormalizeAssetName(Name), true);
    }
    catch (const std::exception& Error)
    {
        UE_DEBUG_LOG_ERROR(Core, "Asset file deletion failed: %s", Error.what());
        return false;
    }
}

void UAssetManager::Clear()
{
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
