#pragma once

#include "Core/AssetSystem/AssetManager.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/Asset/FontAtlasAsset.h"
#include "Core/enum.h"

class FFileManager;

// Built-in asset catalog. GPU resources live in assets, never in raw buffer maps.
class FRenderAssets
{
public:
    void LoadLoadingScreen(URenderer& Renderer, FFileManager& Files);
    void LoadSceneAssets(URenderer& Renderer, FFileManager& Files);
    void Clear();

    TSharedPtr<UStaticMeshAsset> GetMesh(EPrimitive Primitive, bool Textured = false) const;
    TSharedPtr<UTexture2DAsset> GetTexture(EPrimitive Primitive) const;
    TSharedPtr<UFontAtlasAsset> GetDefaultFont() const { return DefaultFont; }
    TSharedPtr<UStaticMeshAsset> GetParticleMesh() const { return ParticleMesh; }
    TSharedPtr<UStaticMeshAsset> GetFullscreenMesh() const { return FullscreenMesh; }
    TSharedPtr<UTexture2DAsset> GetLoadingScreen() const { return LoadingScreen; }

private:
    FAssetManager Manager;
    TMap<EPrimitive, TSharedPtr<UStaticMeshAsset>> Meshes;
    TMap<EPrimitive, TSharedPtr<UStaticMeshAsset>> TexturedMeshes;
    TMap<EPrimitive, TSharedPtr<UTexture2DAsset>> Textures;
    TSharedPtr<UStaticMeshAsset> ParticleMesh;
    TSharedPtr<UStaticMeshAsset> FullscreenMesh;
    TSharedPtr<UTexture2DAsset> LoadingScreen;
    TSharedPtr<UFontAtlasAsset> DefaultFont;
};
