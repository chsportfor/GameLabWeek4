#pragma once

struct FName;
class UAssetManager;
struct FClassInfo;

namespace AssetDragDrop
{
    // Call immediately after the source item. ImGui owns a copy of the UTF-8 path.
    void Source(const char* AssetPath, const char* DisplayName);

    // Return true only when a registered asset of the exact class is dropped.
    bool Slot(const char* Id, const char* TypeLabel, const char* CurrentPath,
        const FClassInfo* ExpectedClass, const UAssetManager& Assets, FName& DroppedName);
}
