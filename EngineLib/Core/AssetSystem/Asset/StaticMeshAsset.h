#pragma once
#include "Material.h"
#include "Core/Math/FBoundingBox.h"
#include "MeshGeometry.h"
#include <functional>

class URenderer;
class UStaticMeshAsset : public UAsset
{
    DECLARE_OBJECT(UStaticMeshAsset, UAsset)
    DECLARE_ASSET_TYPE(UStaticMeshAsset)
public:
    void Load(const std::filesystem::path& Path, UAssetManager& Assets, URenderer& Renderer) override;
    static FName GetDefaultAssetName();
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
    std::unique_ptr<FMeshGeometry> CpuGeometry;
    TArray<UMaterial*> Materials;
    std::function<bool(FMeshGeometry&)> GeometryLoader;
    uint64 GeometrySignature = 0;
};
