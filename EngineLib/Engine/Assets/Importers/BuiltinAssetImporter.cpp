#include "BuiltinAssetImporter.h"
#include "Texture2DImporter.h"
#include "FontAtlasImporter.h"
#include "Core/AssetSystem/AssetFile/StaticMeshAssetFile.h"
#include "Core/AssetSystem/AssetFile/MaterialAssetFile.h"
#include "Core/AssetSystem/AssetFile/Texture2DAssetFile.h"
#include "Core/IO/FileManager.h"
#include "Rendering/Renderer.h"
#include "Rendering/BuiltinAssetNames.h"
#include "Rendering/Primitives/Cube.h"
#include "Rendering/Primitives/Sphere.h"
#include "Rendering/Primitives/Triangle.h"
#include "Rendering/Primitives/GizmoArrow.h"
#include "Rendering/Primitives/Circle.h"
#include "Rendering/Primitives/Primitives.h"
#include "Editor/Console.h"
#include <DirectXTex.h>
#include <stdexcept>

bool FBuiltinAssetImporter::ImportMissingBuiltins(URenderer& Renderer)
{
    try
    {
        auto Missing = [](const char* Name) { return !std::filesystem::exists(FFileManager::Get().ResolvePath(Name)); };
        auto Write = [&](const char* Name, const auto& File)
        {
            if (!Missing(Name)) return;
            const auto Bytes = AssetFile::Serialize(File);
            if (WriteImportedAsset(Name, {Bytes.GetData(), size_t(Bytes.Num())}).IsEmpty())
                throw std::runtime_error(std::string("Builtin import failed: ") + Name);
        };
        if (Missing(BuiltinAssetNames::DefaultTexture))
        {
            uint32 White = 0xffffffff;
            DirectX::Image Image{1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4,
                reinterpret_cast<uint8_t*>(&White)};
            DirectX::Blob DDS;
            if (FAILED(DirectX::SaveToDDSMemory(Image, DirectX::DDS_FLAGS_NONE, DDS)))
                throw std::runtime_error("Cannot encode default white texture");
            if (!Renderer.CreateTexture2DFromMemory(DDS.GetBufferPointer(), DDS.GetBufferSize()))
                throw std::runtime_error("Cannot upload default white texture");
            FTexture2D_uasset File;
            File.bStandalone = true;
            File.Data.SetNum(static_cast<int32>(DDS.GetBufferSize()));
            std::memcpy(File.Data.GetData(), DDS.GetBufferPointer(), DDS.GetBufferSize());
            Write(BuiltinAssetNames::DefaultTexture, File);
        }
        FMaterial_uasset Material;
        Material.bStandalone = true;
        Write(BuiltinAssetNames::DefaultMaterial, Material);

        auto Mesh = [&](const char* Name, const auto& Vertices, const auto& Indices)
        {
            if (!Missing(Name)) return;
            FStaticMesh_uasset File;
            File.bStandalone = true;
            for (const auto& Vertex : Vertices) File.Geometry.Vertices.Add(Vertex);
            for (const auto Index : Indices) File.Geometry.Indices.Add(Index);
            File.Bounds = FBoundingBox(Vertices[0].GetPosition(), Vertices[0].GetPosition());
            for (const auto& Vertex : Vertices) File.Bounds.ExpandToInclude(Vertex.GetPosition());
            File.Sections.Add({0, uint32(File.Geometry.Indices.Num()), 0});
            File.MaterialPaths.Add(FString(BuiltinAssetNames::DefaultMaterial));
            if (!Renderer.CreateVertexBuffer(File.Geometry.Vertices.GetData(), File.Geometry.Vertices.Num()) ||
                !Renderer.CreateIndexBuffer(File.Geometry.Indices.GetData(), File.Geometry.Indices.Num()))
                throw std::runtime_error("Builtin mesh GPU validation failed");
            Write(Name, File);
        };
        Mesh(BuiltinAssetNames::CubeMesh, Cube_vertices, Cube_indices);
        Mesh(BuiltinAssetNames::SphereMesh, Sphere_vertices, Sphere_indices);
        Mesh(BuiltinAssetNames::TriangleMesh, Triangle_vertices, Triangle_indices);
        Mesh(BuiltinAssetNames::GizmoArrowMesh, GizmoArrow_vertices, GizmoArrow_indices);
        Mesh(BuiltinAssetNames::CircleMesh, Circle_vertices, Circle_indices);
        Mesh(BuiltinAssetNames::QuadMesh, Quad_vertices, Quad_indices);
        Mesh(BuiltinAssetNames::FullscreenMesh, Fullscreen_vertices, Fullscreen_indices);

        auto Texture = [&](const char* Source, const char* Name)
        {
            if (Missing(Name) && FTexture2DImporter::ImportUTexture2D(Renderer, Source, Name).IsEmpty())
                throw std::runtime_error(std::string("Builtin texture import failed: ") + Source);
        };
        Texture("Textures/CubeTextureSample.dds", BuiltinAssetNames::CubeTexture);
        Texture("Textures/EarthTexture.dds", BuiltinAssetNames::EarthTexture);
        Texture("Textures/Explosion_Alpha.dds", BuiltinAssetNames::ExplosionTexture);
        Texture("Textures/LoadingScreen.dds", BuiltinAssetNames::LoadingScreen);
        if (Missing(BuiltinAssetNames::DefaultFont) && FFontAtlasImporter::ImportUFontAtlas(Renderer,
            "Fonts/KoreanFullAtlas.png", "Fonts/KoreanFullAtlas.json", BuiltinAssetNames::DefaultFont).IsEmpty())
            throw std::runtime_error("Builtin font import failed");
        return true;
    }
    catch (const std::exception& Error)
    {
        UE_DEBUG_LOG_ERROR(Core, "Builtin asset import failed: %s", Error.what());
        return false;
    }
}
