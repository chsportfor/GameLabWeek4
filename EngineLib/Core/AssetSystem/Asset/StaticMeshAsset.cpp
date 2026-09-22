#include "StaticMeshAsset.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/IO/FileManager.h"
#include "Rendering/BuiltinAssetNames.h"
#include "Core/AssetSystem/AssetFile/StaticMeshAssetFile.h"
#include "Editor/Console.h"
#include "Rendering/Renderer.h"

IMPLEMENT_CLASS(UStaticMeshAsset, UAsset);
IMPLEMENT_ASSET_FILE_SCHEMA(UStaticMeshAsset, AssetFile::GetStaticMeshFileSchema);

namespace
{
    FStaticMesh_uasset ReadMeshAsset(const std::filesystem::path& Path)
    {
        const auto Bytes = FFileManager::Get().ReadFileToString(Path);
        return AssetFile::DeserializeStaticMesh({reinterpret_cast<const uint8*>(Bytes.CStr()), size_t(Bytes.Len())});
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
	FRenderStats& rdst = Renderer.GetMutableRenderStats();
	D3D11_BUFFER_DESC vertexdesc;
	VertexBuffer->GetDesc(&vertexdesc);
	rdst.StaticmeshMemoryByte += vertexdesc.ByteWidth;
	if (IndexBuffer)
	{
		D3D11_BUFFER_DESC indexdesc;
		IndexBuffer->GetDesc(&indexdesc);
		rdst.StaticmeshMemoryByte += indexdesc.ByteWidth;
	}
	VertexCount = Geometry.Vertices.Num();
    IndexCount = Geometry.Indices.Num();
    Sections = InSections;
    Materials = InMaterials;
    BoundingBox = FBoundingBox(Geometry.Vertices[0].GetPosition(), Geometry.Vertices[0].GetPosition());
    for (const auto& vertex : Geometry.Vertices) BoundingBox.ExpandToInclude(vertex.GetPosition());
    GeometrySignature = HashGeometry(Geometry);
    GeometryLoader = std::move(InGeometryLoader);
    CpuGeometry = std::make_unique<FMeshGeometry>(Geometry);
	rdst.StaticmeshResourceCount++;
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

FName UStaticMeshAsset::GetDefaultAssetName() { return FName(BuiltinAssetNames::CubeMesh); }

void UStaticMeshAsset::Load(const std::filesystem::path& Path, UAssetManager& Assets, URenderer& Renderer)
{
    const auto File = ReadMeshAsset(Path);
    TArray<UMaterial*> Materials;
    for (const auto& MaterialPath : File.MaterialPaths)
    {
        auto* Material = Assets.GetAssetAs<UMaterial>(FName(MaterialPath), true);
        if (!Material) throw std::runtime_error("Static mesh material dependency could not be loaded");
        Materials.Add(Material);
    }
    auto ReloadGeometry = [Path](FMeshGeometry& Out)
    {
        try { Out = ReadMeshAsset(Path).Geometry; return true; }
        catch (const std::exception& Error)
        {
            UE_DEBUG_LOG_ERROR(Core, "Static mesh CPU geometry reload failed: %s", Error.what());
            return false;
        }
    };
    Initialize(Renderer, File.Geometry, File.Sections, Materials, ReloadGeometry);
}
