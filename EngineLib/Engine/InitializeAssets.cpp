#include "InitializeAssets.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/Asset/FontAtlasAsset.h"
#include "Core/AssetSystem/AssetSource/StaticMeshAssetSource.h"
#include "Core/AssetSystem/AssetSource/FontAtlasAssetSource.h"
#include "Rendering/BuiltinAssetNames.h"
#include "Rendering/Renderer.h"
#include "Rendering/Primitives/Cube.h"
#include "Rendering/Primitives/Sphere.h"
#include "Rendering/Primitives/GizmoArrow.h"
#include "Rendering/Primitives/Circle.h"
#include "Rendering/Primitives/Triangle.h"
#include "Rendering/Primitives/Primitives.h"

void RegisterLoadingScreenAssets(FAssetManager& Assets, URenderer& Renderer, FFileManager& Files)
{
    Assets.RegisterAsset(BuiltinAssetNames::LoadingScreen, MakeShared<FTexture2DAssetLoader>(Renderer.GetDevice()),
        MakeShared<FFileAssetSource>(Files, "Textures/LoadingScreen.dds"));
    Assets.RegisterAsset(BuiltinAssetNames::FullscreenMesh, MakeShared<FStaticMeshAssetLoader>(Renderer),
        MakeShared<FStaticMeshAssetSource>(Fullscreen_vertices, Fullscreen_indices));
}

void RegisterSceneAssets(FAssetManager& Assets, URenderer& Renderer, FFileManager& Files)
{
    auto MeshLoader = MakeShared<FStaticMeshAssetLoader>(Renderer);
    auto RegisterMesh = [&](EPrimitive Type, const auto& Vertices, const auto& Indices)
    {
        Assets.RegisterAsset(BuiltinAssetNames::Mesh(Type), MeshLoader,
            MakeShared<FStaticMeshAssetSource>(Vertices, Indices));
    };
    RegisterMesh(EPrimitive::EP_Cube, Cube_vertices, Cube_indices);
    RegisterMesh(EPrimitive::EP_Sphere, Sphere_vertices, Sphere_indices);
    RegisterMesh(EPrimitive::EP_GizmoArrow, GizmoArrow_vertices, GizmoArrow_indices);
    RegisterMesh(EPrimitive::EP_Circle, Circle_vertices, Circle_indices);
    RegisterMesh(EPrimitive::EP_Triangle, Triangle_vertices, Triangle_indices);
    RegisterMesh(EPrimitive::EP_BillboardQuad, Quad_vertices, Quad_indices);

    auto TextureLoader = MakeShared<FTexture2DAssetLoader>(Renderer.GetDevice());
    Assets.RegisterAsset(BuiltinAssetNames::Texture(EPrimitive::EP_Cube), TextureLoader,
        MakeShared<FFileAssetSource>(Files, "Textures/CubeTextureSample.dds"));
    Assets.RegisterAsset(BuiltinAssetNames::Texture(EPrimitive::EP_Sphere), TextureLoader,
        MakeShared<FFileAssetSource>(Files, "Textures/EarthTexture.dds"));
    Assets.RegisterAsset(BuiltinAssetNames::Texture(EPrimitive::EP_BillboardQuad), TextureLoader,
        MakeShared<FFileAssetSource>(Files, "Textures/Explosion_Alpha.dds"));
    Assets.RegisterAsset(BuiltinAssetNames::DefaultFont, MakeShared<FFontAtlasAssetLoader>(Renderer.GetDevice()),
        MakeShared<FFontAtlasAssetSource>(Files, "Fonts/KoreanFullAtlas.png", "Fonts/KoreanFullAtlas.json"));
}
