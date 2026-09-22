#pragma once
#include "AssetImporter.h"
class URenderer;

// Startup asset preparation: creates missing builtin files before registration.
// Existing .uasset files are preserved. Also callable from the development tool.
class FBuiltinAssetImporter : private FAssetImporter
{
public:
    // Startup preparation: true also means every builtin already existed (nothing to import).
    static bool ImportMissingBuiltins(URenderer& Renderer);
};
