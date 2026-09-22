#pragma once

#include "AssetImporter.h"
#include "Core/Math/Color.h"
#include <vector>

class URenderer;
struct FObjMaterial;

class FMaterialImporter : private FAssetImporter
{
    friend class FStaticMeshImporter;
    struct FPreparedMaterials
    {
        std::vector<FPreparedAssetFile> Files;
        TArray<FString> MaterialPaths;
    };
    // No disk writes. Shared by MTL and mesh imports so all outputs commit together.
    static FPreparedMaterials PrepareMaterials(URenderer& Renderer, const TArray<FObjMaterial>& Materials,
        const std::filesystem::path& MaterialDirectory, const std::filesystem::path& TextureDirectory,
        bool bStandalone);
public:
    // Prepare image + material together; no orphan texture on a failed material import.
    // Texture is non-Standalone, stored in Assets/Textures/<image stem>.uasset.
    static TArray<FName> ImportUMaterialFromImage(URenderer& Renderer,
        const std::filesystem::path& ImagePath, const FLinearColor& DiffuseColor,
        const std::filesystem::path& Destination, bool bStandalone = true);

    // One material from an existing texture .uasset (or empty path for a color-only material).
    // Destination must name a .uasset file. Relative paths use the asset root.
    static TArray<FName> ImportUMaterial(const std::filesystem::path& TextureAssetPath,
        const FLinearColor& DiffuseColor, const std::filesystem::path& Destination,
        bool bStandalone = true);

    // Imports every newmtl entry; defaults to Assets/Materials/<material>.uasset.
    // map_Kd images become non-Standalone texture assets in Assets/Textures/.
    // Shared source textures are imported once per call. All new outputs commit together.
    static TArray<FName> ImportUMaterial(URenderer& Renderer, const std::filesystem::path& MtlPath,
        const std::filesystem::path& DestinationDirectory = "Materials", bool bStandalone = true);
};
