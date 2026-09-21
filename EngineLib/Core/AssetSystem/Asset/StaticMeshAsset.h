#pragma once
#include "Material.h"
#include "Core/Math/FBoundingBox.h"
#include "Rendering/VertexType.h"
#include <functional>

class URenderer;
struct FMeshSection
{
    uint32 FirstIndex = 0;
    uint32 IndexCount = 0;
    uint32 MaterialIndex = 0;
};

struct FMeshGeometry
{
    TArray<FVertexSimple> Vertices;
    TArray<uint32> Indices;
};

class UStaticMeshAsset : public UAsset
{
    DECLARE_OBJECT(UStaticMeshAsset, UAsset)
    DECLARE_ASSET_TYPE(UStaticMeshAsset)
public:
    void Initialize(URenderer& Renderer, const FMeshGeometry& Geometry,
        const TArray<FMeshSection>& InSections, const TArray<UMaterial*>& InMaterials,
        std::function<bool(FMeshGeometry&)> InGeometryLoader);
    Microsoft::WRL::ComPtr<ID3D11Buffer> GetVertexBuffer() const { return VertexBuffer; }
    Microsoft::WRL::ComPtr<ID3D11Buffer> GetIndexBuffer() const { return IndexBuffer; }
    uint32 GetVertexCount() const { return VertexCount; }
    uint32 GetIndexCount() const { return IndexCount; }
    const FBoundingBox& GetLocalBoundingBox() const { return BoundingBox; }
    const TArray<FMeshSection>& GetSections() const { return Sections; }
    const TArray<UMaterial*>& GetMaterials() const { return Materials; }
    UMaterial* GetMaterial(int32 Slot) const;

    // Loaded assets retain CPU geometry until explicitly unloaded.
    bool LoadCpuGeometry();
    void UnloadCpuGeometry() { CpuGeometry.reset(); }
    const FMeshGeometry* GetCpuGeometry() const { return CpuGeometry.get(); }
private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
    uint32 VertexCount = 0;
    uint32 IndexCount = 0;
    FBoundingBox BoundingBox;
    TArray<FMeshSection> Sections;
    TArray<UMaterial*> Materials;
    std::unique_ptr<FMeshGeometry> CpuGeometry;
    std::function<bool(FMeshGeometry&)> GeometryLoader;
    uint64 GeometrySignature = 0;
};

class FStaticMeshAssetLoader_Primitive : public FAssetLoader
{
    DECLARE_ASSET_LOADER_TYPE(UStaticMeshAsset)
public:
    FStaticMeshAssetLoader_Primitive(URenderer& Renderer, UAssetManager& Assets) : Renderer(Renderer), Assets(Assets) {}
    UAsset* LoadAsset(const FName& Name, FAssetSource& Source) override;
private:
    URenderer& Renderer;
    UAssetManager& Assets;
};

class FStaticMeshAssetLoader_File : public FAssetLoader
{
    DECLARE_ASSET_LOADER_TYPE(UStaticMeshAsset)
public:
    FStaticMeshAssetLoader_File(URenderer& Renderer, UAssetManager& Assets) : Renderer(Renderer), Assets(Assets) {}
    UAsset* LoadAsset(const FName& Name, FAssetSource& Source) override;
private:
    URenderer& Renderer;
    UAssetManager& Assets;
};
