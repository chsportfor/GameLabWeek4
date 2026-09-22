#include "AssetFileSchema.h"
#include "AssetFileDocument.h"
#include "AssetFileJson.h"
#include <Windows.h>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <system_error>

namespace
{
    namespace fs = std::filesystem;
    auto& Schemas()
    {
        static std::map<const FClassInfo*, const FAssetFileSchema*> Values;
        return Values;
    }
    TArray<uint8> ReadBytes(const fs::path& Path)
    {
        std::ifstream Stream(Path, std::ios::binary | std::ios::ate);
        if (!Stream) throw std::runtime_error("Cannot open asset for upgrade");
        const auto Size = Stream.tellg();
        if (Size < 0 || Size > (std::numeric_limits<int32>::max)()) throw std::runtime_error("Invalid asset size");
        Stream.seekg(0);
        TArray<uint8> Bytes; Bytes.SetNum(static_cast<int32>(Size));
        if (!Stream.read(reinterpret_cast<char*>(Bytes.GetData()), Bytes.Num()))
            throw std::runtime_error("Cannot read asset for upgrade");
        return Bytes;
    }
    FAssetFileDocument Parse(const TArray<uint8>& Bytes, const char* ExpectedClass)
    {
        FAssetFileDocument Document;
        std::span<const uint8> Cursor{Bytes.GetData(), size_t(Bytes.Num())};
        Document.Body = AssetFile::Detail::ReadDocument(Cursor, Document.Header, ExpectedClass);
        AssetFile::Detail::Append(Document.Payload, Cursor);
        return Document;
    }
    [[noreturn]] void WindowsError(const char* Operation)
    {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), Operation);
    }
    void WriteNewFile(const fs::path& Path, const TArray<uint8>& Bytes)
    {
        HANDLE File = CreateFileW(Path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (File == INVALID_HANDLE_VALUE) WindowsError("Create asset upgrade file");
        try
        {
            DWORD Written = 0;
            if (!WriteFile(File, Bytes.GetData(), static_cast<DWORD>(Bytes.Num()), &Written, nullptr) ||
                Written != static_cast<DWORD>(Bytes.Num())) WindowsError("Write asset upgrade file");
            if (!FlushFileBuffers(File)) WindowsError("Flush asset upgrade file");
            CloseHandle(File);
        }
        catch (...)
        {
            CloseHandle(File);
            std::error_code Ignored; fs::remove(Path, Ignored); // Only the file created above.
            throw;
        }
    }
    fs::path BackupPath(const fs::path& Path, uint32 Version)
    {
        fs::path Base = Path; Base += L".schema-v" + std::to_wstring(Version) + L".bak";
        auto Candidate = Base;
        for (uint32 I = 1; fs::exists(Candidate); ++I)
        {
            Candidate = Base; Candidate += L"." + std::to_wstring(I);
        }
        return Candidate;
    }
}

void AssetFile::SetStandalone(const fs::path& Path, bool Standalone)
{
    const auto Original = ReadBytes(Path);
    std::span<const uint8> Body{Original.GetData(), size_t(Original.Num())};
    uint64 BodyLength = 0;
    auto Header = ReadHeader(Body, &BodyLength);
    if (BodyLength > Body.size()) throw std::runtime_error("Truncated asset body");
    if (Header.bStandalone == Standalone) return;
    Header.bStandalone = Standalone;
    auto Updated = SerializeHeader(Header, BodyLength);
    Detail::Append(Updated, Body);
    fs::path Temporary = Path; Temporary += L".metadata.tmp";
    WriteNewFile(Temporary, Updated);
    try
    {
        const auto Current = ReadBytes(Path);
        if (Current.Num() != Original.Num() || std::memcmp(Current.GetData(), Original.GetData(), Current.Num()))
            throw std::runtime_error("Asset changed externally during metadata update");
        if (!MoveFileExW(Temporary.c_str(), Path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            WindowsError("Replace asset metadata");
    }
    catch (...)
    {
        std::error_code Ignored; fs::remove(Temporary, Ignored);
        throw;
    }
}

bool AssetFile::RegisterSchema(const FClassInfo* Class, const FAssetFileSchema& Schema)
{
    if (!Class || !Schema.LatestVersion || !Schema.UpgradeToLatest || !Schema.RebuildDependencies || !Schema.Validate)
        throw std::invalid_argument("Incomplete asset file schema registration");
    if (!Schemas().emplace(Class, &Schema).second) throw std::logic_error("Duplicate asset file schema");
    return true;
}
const FAssetFileSchema& AssetFile::GetSchema(const FClassInfo* Class)
{
    const auto It = Schemas().find(Class);
    if (It == Schemas().end()) throw std::runtime_error("Asset class has no registered file schema");
    return *It->second;
}
FFile_uasset AssetFile::UpgradeFile(const fs::path& Path, const FFile_uasset& Header, const FAssetFileSchema& Schema)
{
    if (!Header.SchemaVersion || Header.SchemaVersion > Schema.LatestVersion)
        throw std::runtime_error("Unsupported asset schema version (file is newer than this program)");
    if (Header.SchemaVersion == Schema.LatestVersion) return Header;

    const auto Original = ReadBytes(Path);
    auto Document = Parse(Original, Header.AssetType.CStr());
    if (Document.Header.SchemaVersion != Header.SchemaVersion)
        throw std::runtime_error("Asset version changed during scan");
    const bool Standalone = Document.Header.bStandalone;
    Schema.UpgradeToLatest(Document);
    if (Document.Header.SchemaVersion != Schema.LatestVersion ||
        std::string_view(Document.Header.AssetType.CStr()) != Header.AssetType.CStr() ||
        Document.Header.bStandalone != Standalone)
        throw std::runtime_error("Upgrade must reach latest version and preserve asset identity/Standalone");
    Detail::Object(Document.Body);
    Schema.RebuildDependencies(Document);
    Schema.Validate(Document);
    const auto Updated = Detail::WriteDocument(Document.Header, Document.Body,
        {Document.Payload.GetData(), size_t(Document.Payload.Num())});

    fs::path Temporary = Path; Temporary += L".upgrade.tmp";
    // A leftover/existing temp file is never overwritten or removed by this call.
    WriteNewFile(Temporary, Updated);
    try
    {
        const auto Written = ReadBytes(Temporary);
        if (Written.Num() != Updated.Num() || std::memcmp(Written.GetData(), Updated.GetData(), Written.Num()))
            throw std::runtime_error("Asset upgrade write verification failed");
        const auto Verified = Parse(Written, Header.AssetType.CStr());
        Schema.Validate(Verified);
        const auto Current = ReadBytes(Path);
        if (Current.Num() != Original.Num() || std::memcmp(Current.GetData(), Original.GetData(), Current.Num()))
            throw std::runtime_error("Asset changed externally during upgrade");
        // Backups are not .uasset files and are excluded from the registry scan.
        WriteNewFile(BackupPath(Path, Header.SchemaVersion), Original);
        if (!MoveFileExW(Temporary.c_str(), Path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            WindowsError("Replace upgraded asset");
    }
    catch (...)
    {
        std::error_code Ignored; fs::remove(Temporary, Ignored);
        throw;
    }
    return Document.Header;
}
