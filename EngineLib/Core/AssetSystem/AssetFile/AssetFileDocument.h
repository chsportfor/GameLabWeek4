#pragma once
#include "AssetFile.h"
#include "ThirdParty/Json/json.hpp"

// Editable disk representation, including old fields unknown to today's typed DTO.
// Upgrade only the affected JSON keys; keep unrelated keys and payload bytes intact.
struct FAssetFileDocument
{
    FFile_uasset Header;
    json::JSON Body;
    TArray<uint8> Payload;
};
