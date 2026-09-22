#include "StaticMeshImporter.h"
#include "MaterialImporter.h"
#include "Core/AssetSystem/AssetFile/StaticMeshAssetFile.h"
#include "Engine/Assets/ObjImporter.h"
#include "Editor/Console.h"
#include "Rendering/Renderer.h"
#include <stdexcept>

namespace fs = std::filesystem;

bool FStaticMeshImporter::ImportParsedMesh(URenderer& Renderer, const FStaticMesh& Mesh,
    const fs::path& SourceStem, const fs::path& Destination, bool bStandalone, bool bFlipTextureV)
{
    if (!Renderer.Device) throw std::runtime_error("Static mesh import requires an initialized renderer");
    if (Mesh.Vertices.IsEmpty() || Mesh.Indices.IsEmpty()) throw std::runtime_error("Mesh has no triangles");
    auto Target = FFileManager::Get().ResolvePath(Destination);
    if (Target.extension() != ".uasset") Target /= SourceStem.wstring() + L".uasset";
    const auto DependencyDirectory = Target.parent_path() / Target.stem();

    FStaticMesh_uasset File;
    File.bStandalone = bStandalone;
    File.Geometry.Vertices.Reserve(Mesh.Vertices.Num());
    for (const auto& V : Mesh.Vertices)
    {
        const float TextureV = bFlipTextureV ? 1.0f - V.UV.y : V.UV.y;
        File.Geometry.Vertices.Add({V.Position.x, V.Position.y, V.Position.z, V.Normal.x, V.Normal.y, V.Normal.z,
            V.Color.x, V.Color.y, V.Color.z, V.Color.w, V.UV.x, TextureV});
    }
    File.Geometry.Indices = Mesh.Indices;
    File.Bounds = FBoundingBox(Mesh.Vertices[0].Position, Mesh.Vertices[0].Position);
    for (const auto& V : Mesh.Vertices) File.Bounds.ExpandToInclude(V.Position);
    for (const auto& S : Mesh.Sections) File.Sections.Add({S.FirstIndex, S.NumIndices, S.MaterialIndex});

    // Reuse the existing material/texture preparation, preserving the parser's material slot order.
    TArray<FObjMaterial> Materials;
    for (const auto& M : Mesh.Materials)
    {
        FObjMaterial Parsed;
        Parsed.Name = M.Name;
        Parsed.DiffuseColor = M.DiffuseColor;
        Parsed.DiffuseTexturePath = M.DiffuseTexturePath;
        Parsed.Dissolve = M.Dissolve;
        Parsed.bHasDissolve = M.bHasDissolve;
        Parsed.Transparency = M.Transparency;
        Parsed.bHasTransparency = M.bHasTransparency;
        Materials.Add(Parsed);
    }
    auto Prepared = FMaterialImporter::PrepareMaterials(Renderer, Materials,
        DependencyDirectory / "Materials", DependencyDirectory / "Textures", false);
    File.MaterialPaths = std::move(Prepared.MaterialPaths);
    const auto Bytes = AssetFile::Serialize(File); // Validate geometry, sections and paths before uploading.
    {
        const auto Vertices = Renderer.CreateVertexBuffer(File.Geometry.Vertices.GetData(), File.Geometry.Vertices.Num());
        const auto Indices = Renderer.CreateIndexBuffer(File.Geometry.Indices.GetData(), File.Geometry.Indices.Num());
        if (!Vertices || !Indices) throw std::runtime_error("Static mesh GPU validation failed");
    } // Temporary validation buffers are released before writing any files.

    std::vector<FAssetFileToWrite> Outputs;
    Outputs.reserve(Prepared.Files.size() + 1);
    for (const auto& Dependency : Prepared.Files)
        Outputs.push_back({Dependency.Path, {Dependency.Bytes.GetData(), size_t(Dependency.Bytes.Num())}});
    Outputs.push_back({Target, {Bytes.GetData(), size_t(Bytes.Num())}});
    return WriteImportedAssets(Outputs);
}

bool FStaticMeshImporter::ImportUStaticMesh(URenderer& Renderer, const fs::path& ObjPath,
    const fs::path& Destination, bool bStandalone)
{
    try
    {
        auto& Files = FFileManager::Get();
        const auto Source = Files.ResolvePath(ObjPath);
        FStaticMesh Mesh;
        FString Error;
        if (!FObjImporter::LoadFromFile(Source, Files, Mesh, Error)) throw std::runtime_error(Error.CStr());
        return ImportParsedMesh(Renderer, Mesh, Source.stem(), Destination, bStandalone, false);
    }
    catch (const std::exception& Error)
    {
        UE_LOG(Error, Core, "OBJ static mesh import failed: %s", Error.what());
        return false;
    }
}

bool FStaticMeshImporter::ImportUStaticMeshFromBinary(URenderer& Renderer, const fs::path& BinaryPath,
    const fs::path& Destination, bool bStandalone)
{
    try
    {
        auto& Files = FFileManager::Get();
        const auto Source = Files.ResolvePath(BinaryPath);
        FStaticMesh Mesh;
        FString Error;
        if (!FObjImporter::LoadBinaryFromFile(Source, Files, Mesh, Error)) throw std::runtime_error(Error.CStr());
        const auto ObjStem = Mesh.PathFileName.Len() ? fs::u8path(Mesh.PathFileName.CStr()).stem() : Source.stem();
        if (ObjStem.empty() || ObjStem == "." || ObjStem == "..") throw std::runtime_error("Invalid cached OBJ filename");
        return ImportParsedMesh(Renderer, Mesh, ObjStem, Destination, bStandalone, false);
    }
    catch (const std::exception& Error)
    {
        UE_LOG(Error, Core, "Binary OBJ static mesh import failed: %s", Error.what());
        return false;
    }
}

bool FStaticMeshImporter::ImportUStaticMesh(URenderer& Renderer, const FStaticMesh& Mesh,
    const fs::path& SourceStem, const fs::path& Destination, bool bStandalone, bool bFlipTextureV)
{
    try
    {
        return ImportParsedMesh(Renderer, Mesh, SourceStem, Destination, bStandalone, bFlipTextureV);
    }
    catch (const std::exception& Error)
    {
        UE_LOG(Error, Core, "Parsed OBJ static mesh import failed: %s", Error.what());
        return false;
    }
}
