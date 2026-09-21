#include "StaticMeshAsset.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/AssetSystem/AssetSource/StaticMeshAssetSource.h"
#include "Engine/Assets/ObjImporter.h"
#include "Rendering/Renderer.h"

IMPLEMENT_CLASS(UStaticMeshAsset, UAsset);

namespace
{
    FMeshGeometry ConvertGeometry(const FStaticMesh& Mesh)
    {
        FMeshGeometry geometry;
        geometry.Vertices.Reserve(Mesh.Vertices.Num());
        for (const auto& v : Mesh.Vertices)
            geometry.Vertices.Add({v.Position.x,v.Position.y,v.Position.z, v.Normal.x,v.Normal.y,v.Normal.z,
                v.Color.x,v.Color.y,v.Color.z,v.Color.w, v.UV.x,v.UV.y});
        geometry.Indices = Mesh.Indices;
        return geometry;
    }

    uint64 HashGeometry(const FMeshGeometry& Geometry)
    {
        uint64 hash = 14695981039346656037ull;
        auto add = [&](const void* data, size_t size)
        {
            const auto* bytes = static_cast<const uint8*>(data);
            for (size_t i = 0; i < size; ++i) { hash ^= bytes[i]; hash *= 1099511628211ull; }
        };
        add(Geometry.Vertices.GetData(), Geometry.Vertices.Num() * sizeof(FVertexSimple));
        add(Geometry.Indices.GetData(), Geometry.Indices.Num() * sizeof(uint32));
        return hash;
    }
}

void UStaticMeshAsset::Initialize(URenderer& Renderer, const FMeshGeometry& Geometry,
    const TArray<FMeshSection>& InSections, const TArray<UMaterial*>& InMaterials,
    std::function<bool(FMeshGeometry&)> InGeometryLoader)
{
    if (Geometry.Vertices.IsEmpty()) throw std::invalid_argument("Missing mesh vertices");
    for (uint32 index : Geometry.Indices)
        if (index >= Geometry.Vertices.Num()) throw std::out_of_range("Mesh vertex index out of range");
    for (const auto& section : InSections)
    {
        if (section.FirstIndex > Geometry.Indices.Num() || section.IndexCount > Geometry.Indices.Num() - section.FirstIndex)
            throw std::out_of_range("Mesh section index range invalid");
        if (section.MaterialIndex >= InMaterials.Num() || !InMaterials[section.MaterialIndex])
            throw std::out_of_range("Mesh section material invalid");
    }
    VertexBuffer = Renderer.CreateVertexBuffer(Geometry.Vertices.GetData(), Geometry.Vertices.Num());
    IndexBuffer = Renderer.CreateIndexBuffer(Geometry.Indices.GetData(), Geometry.Indices.Num());
    if (!VertexBuffer || (!Geometry.Indices.IsEmpty() && !IndexBuffer)) throw std::runtime_error("Mesh GPU upload failed");
    VertexCount = Geometry.Vertices.Num();
    IndexCount = Geometry.Indices.Num();
    Sections = InSections;
    Materials = InMaterials;
    BoundingBox = FBoundingBox(Geometry.Vertices[0].GetPosition(), Geometry.Vertices[0].GetPosition());
    for (const auto& vertex : Geometry.Vertices) BoundingBox.ExpandToInclude(vertex.GetPosition());
    GeometrySignature = HashGeometry(Geometry);
    GeometryLoader = std::move(InGeometryLoader);
    CpuGeometry = std::make_unique<FMeshGeometry>(Geometry);
}

UMaterial* UStaticMeshAsset::GetMaterial(int32 Slot) const
{
    return Slot >= 0 && Slot < Materials.Num() ? Materials[Slot] : nullptr;
}

bool UStaticMeshAsset::LoadCpuGeometry()
{
    if (CpuGeometry) return true;
    if (!GeometryLoader) return false;
    auto geometry = std::make_unique<FMeshGeometry>();
    if (!GeometryLoader(*geometry)) return false;
    // Do not expose geometry from an edited source alongside older GPU buffers.
    if (geometry->Vertices.Num() != VertexCount || geometry->Indices.Num() != IndexCount ||
        HashGeometry(*geometry) != GeometrySignature) return false;
    CpuGeometry = std::move(geometry);
    return true;
}

UAsset* FStaticMeshAssetLoader_Primitive::LoadAsset(const FName& Name, FAssetSource& Source)
{
    const auto source = static_cast<const FStaticMeshAssetSource&>(Source);
    auto loadGeometry = [source](FMeshGeometry& geometry)
    {
        for (const auto& vertex : source.Vertices) geometry.Vertices.Add(vertex);
        for (const auto index : source.Indices) geometry.Indices.Add(index);
        return !geometry.Vertices.IsEmpty();
    };
    FMeshGeometry geometry;
    if (!loadGeometry(geometry)) return nullptr;
    TArray<FMeshSection> sections;
    sections.Add({0, static_cast<uint32>(geometry.Indices.Num()), 0});
    TArray<UMaterial*> materials;
    materials.Add(GetDefaultMaterial(Assets));
    std::unique_ptr<UStaticMeshAsset> asset(FObjectFactory::ConstructUnInitializedObject<UStaticMeshAsset>(Name));
    asset->Initialize(Renderer, geometry, sections, materials, loadGeometry);
    return asset.release();
}

UAsset* FStaticMeshAssetLoader_File::LoadAsset(const FName& Name, FAssetSource& Source)
{
    const auto source = static_cast<const FFileAssetSource&>(Source);
    FStaticMesh parsed;
    FString error;
    if (!FObjImporter::LoadFromFile(source.FilePath.string(), source.FileManager, parsed, error))
        throw std::runtime_error(error.CStr());
    FMeshGeometry geometry = ConvertGeometry(parsed);
    TArray<UMaterial*> materials;
    for (const auto& material : parsed.Materials)
    {
        if (!material.MaterialLibraryPath.Len()) { materials.Add(GetDefaultMaterial(Assets)); continue; }
        const std::filesystem::path path(material.MaterialLibraryPath.CStr());
        const auto materialName = UAssetManager::MakeSubAssetName(
            UAssetManager::MakeFileAssetName(path, source.FileManager), material.Name);
        Assets.RegisterAsset(materialName, MakeShared<FMaterialAssetLoader>(Renderer.GetDevice(), Assets),
            MakeShared<FMaterialAssetSource>(source.FileManager, path, material.Name));
        auto* loaded = Assets.GetAssetAs<UMaterial>(materialName, true);
        if (!loaded) return nullptr;
        materials.Add(loaded);
    }
    TArray<FMeshSection> sections;
    for (const auto& section : parsed.Sections) sections.Add({section.FirstIndex, section.NumIndices, section.MaterialIndex});
    auto loadGeometry = [source](FMeshGeometry& out)
    {
        FStaticMesh mesh;
        FString error;
        if (!FObjImporter::LoadFromFile(source.FilePath.string(), source.FileManager, mesh, error)) return false;
        out = ConvertGeometry(mesh);
        return true;
    };
    std::unique_ptr<UStaticMeshAsset> asset(FObjectFactory::ConstructUnInitializedObject<UStaticMeshAsset>(Name));
    asset->Initialize(Renderer, geometry, sections, materials, loadGeometry);
    return asset.release();
}
