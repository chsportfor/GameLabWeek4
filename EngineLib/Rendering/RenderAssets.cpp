#include "RenderAssets.h"

#include "Core/AssetSystem/AssetSource/StaticMeshAssetSource.h"
#include "Core/AssetSystem/AssetSource/FontAtlasAssetSource.h"
#include "Core/IO/FileManager.h"
#include "Rendering/Renderer.h"
#include "Rendering/Primitives/Cube.h"
#include "Rendering/Primitives/Sphere.h"
#include "Rendering/Primitives/GizmoArrow.h"
#include "Rendering/Primitives/Circle.h"
#include "Rendering/Primitives/Triangle.h"
#include "Rendering/Primitives/Primitives.h"
#include "Rendering/Primitives/TexturedPrimitives.h"

void FRenderAssets::LoadLoadingScreen(URenderer& Renderer, FFileManager& Files)
{
    FTexture2DAssetLoader TextureLoader(Renderer.GetDevice());
    FFileAssetSource TextureSource(Files, "Textures/LoadingScreen.dds");
    LoadingScreen = Manager.Load<UTexture2DAsset>("Texture.LoadingScreen", TextureLoader, TextureSource);
    FStaticMeshAssetLoader MeshLoader(Renderer);
    FStaticMeshAssetSource MeshSource(Fullscreen_vertices, Fullscreen_indices);
    FullscreenMesh = Manager.Load<UStaticMeshAsset>("Mesh.Fullscreen", MeshLoader, MeshSource);
}

void FRenderAssets::LoadSceneAssets(URenderer& Renderer, FFileManager& Files)
{
    FStaticMeshAssetLoader MeshLoader(Renderer);
    auto LoadMesh = [&](const char* Name, const auto& Vertices, const auto& Indices)
    {
        FStaticMeshAssetSource Source(Vertices, Indices);
        return Manager.Load<UStaticMeshAsset>(Name, MeshLoader, Source);
    };
    Meshes.Add(EPrimitive::EP_Cube, LoadMesh("Mesh.Cube", Cube_vertices, Cube_indices));
    Meshes.Add(EPrimitive::EP_Sphere, LoadMesh("Mesh.Sphere", Sphere_vertices, Sphere_indices));
    Meshes.Add(EPrimitive::EP_GizmoArrow, LoadMesh("Mesh.GizmoArrow", GizmoArrow_vertices, GizmoArrow_indices));
    Meshes.Add(EPrimitive::EP_Circle, LoadMesh("Mesh.Circle", Circle_vertices, Circle_indices));
    Meshes.Add(EPrimitive::EP_Triangle, LoadMesh("Mesh.Triangle", Triangle_vertices, Triangle_indices));
    Meshes.Add(EPrimitive::EP_BillboardQuad, LoadMesh("Mesh.Quad", Quad_vertices, Quad_indices));
    TexturedMeshes.Add(EPrimitive::EP_Cube, LoadMesh("Mesh.Cube.Textured", CubeTextureVertices, CubeTextureIndices));
    TexturedMeshes.Add(EPrimitive::EP_Sphere, LoadMesh("Mesh.Sphere.Textured", SphereTextureVertices, SphereTextureIndices));
    TexturedMeshes.Add(EPrimitive::EP_BillboardQuad, GetMesh(EPrimitive::EP_BillboardQuad));
    ParticleMesh = LoadMesh("Mesh.Particle", QuadTextureIndexedVertices, QuadTextureIndexedIndices);

    FTexture2DAssetLoader TextureLoader(Renderer.GetDevice());
    auto LoadTexture = [&](EPrimitive Primitive, const char* Name, const char* Path)
    {
        FFileAssetSource Source(Files, Path);
        Textures.Add(Primitive, Manager.Load<UTexture2DAsset>(Name, TextureLoader, Source));
    };
    LoadTexture(EPrimitive::EP_Cube, "Texture.Cube", "Textures/CubeTextureSample.dds");
    LoadTexture(EPrimitive::EP_Sphere, "Texture.Earth", "Textures/EarthTexture.dds");
    LoadTexture(EPrimitive::EP_BillboardQuad, "Texture.Explosion", "Textures/Explosion_Alpha.dds");

    FFontAtlasAssetLoader FontLoader(Renderer.GetDevice());
    FFontAtlasAssetSource FontSource(Files, "Fonts/KoreanFullAtlas.png", "Fonts/KoreanFullAtlas.json");
    DefaultFont = Manager.Load<UFontAtlasAsset>("Font.Korean", FontLoader, FontSource);
}

TSharedPtr<UStaticMeshAsset> FRenderAssets::GetMesh(EPrimitive Primitive, bool Textured) const
{
    const auto* Asset = (Textured ? TexturedMeshes : Meshes).Find(Primitive);
    return Asset ? *Asset : nullptr;
}

TSharedPtr<UTexture2DAsset> FRenderAssets::GetTexture(EPrimitive Primitive) const
{
    const auto* Asset = Textures.Find(Primitive);
    return Asset ? *Asset : nullptr;
}

void FRenderAssets::Clear()
{
    Meshes.Empty();
    TexturedMeshes.Empty();
    Textures.Empty();
    ParticleMesh.reset();
    FullscreenMesh.reset();
    LoadingScreen.reset();
    DefaultFont.reset();
    Manager.Clear();
}
