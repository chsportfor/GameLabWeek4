#pragma once
#include "AssetFile.h"
#include <filesystem>

struct FClassInfo;
struct FAssetFileDocument;

/* Schema maintenance workflow:
 * 1. Raise LatestVersion in the class's Get*FileSchema() (initial JSON version=1).
 * 2. Add ordered old->next steps to Upgrade*ToLatest(); keep the old steps.
 *    Advance Header.SchemaVersion only AFTER that step has completed.
 * 3. Update the typed DTO/Serialize/Deserialize and RebuildDependencies callback
 *    to describe the latest schema; update the complete format comment in its .h.
 * 4. Add an old-version fixture test. Never redefine a version already written.
 * ReadHeader is read-only; ScanAssets/RegisterAsset explicitly request UpgradeFile.
 * Latest-version registration reads only the header, as before. Old-version
 * registration updates the disk file BEFORE publishing its dependency metadata.
 * File-schema callbacks never construct a UObject or allocate GPU resources.
 */
struct FAssetFileSchema
{
    uint32 LatestVersion;
    void (*UpgradeToLatest)(FAssetFileDocument& Document);
    void (*RebuildDependencies)(FAssetFileDocument& Document);
    void (*Validate)(const FAssetFileDocument& Document);
};

namespace AssetFile
{
    // Schema must have static lifetime; register once per exact class.
    bool RegisterSchema(const FClassInfo* Class, const FAssetFileSchema& Schema);
    const FAssetFileSchema& GetSchema(const FClassInfo* Class);
    // Latest files return immediately without reading their body or writing anything.
    // Old files: upgrade in memory, derive dependencies, validate, write/re-read a
    // temporary file, back up the original, then atomically replace on Windows.
    // Backup: <asset>.schema-vN.bak[.1, .2, ...]; no overwrite of existing backups.
    // Failure throws. Successful files are not rolled back if a later scan fails.
    FFile_uasset UpgradeFile(const std::filesystem::path& Path,
        const FFile_uasset& Header, const FAssetFileSchema& Schema);
}

// Put beside IMPLEMENT_CLASS in the asset's cpp. No central list/switch required.
#define IMPLEMENT_ASSET_FILE_SCHEMA(Class, SchemaGetter) \
    namespace { const bool Class##_FileSchemaRegistered = \
        AssetFile::RegisterSchema(Class::GetClass(), SchemaGetter()); }
