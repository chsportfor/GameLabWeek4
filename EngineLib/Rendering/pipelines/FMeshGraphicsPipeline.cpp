#include "FMeshGraphicsPipeline.h"
#include "Core/AssetSystem/Asset/Texture2DAsset.h"
#include "../Renderer.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include <algorithm>
#include <functional>

namespace
{
    struct FMeshConstants { FMatrix Model; FVector4 Color; int32 UseVertexColor; int32 HasTexture; int32 Padding[2]{}; };
}

FMeshGraphicsPipeline::FMeshGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_BACK, 0, { EViewModeIndex::VMI_Lit, EViewModeIndex::VMI_Wireframe });
    SetDepthStencilState(true, true);
    SetShader("Assets/Shaders/Mesh.hlsl", true);
    AddConstantBuffer<FMeshConstants>();
    AddConstantBuffer<FMatrix>();
    SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);
}

void FMeshGraphicsPipeline::Draw(TArray<FRenderInfo>& Infos, const FMatrix& ViewProjection)
{
    BeginDraw();
    std::sort(Infos.begin(), Infos.end(), [](const FRenderInfo& A, const FRenderInfo& B)
    {
        if (A.Texture.get() != B.Texture.get())
            return std::less<UTexture2DAsset*>{}(A.Texture.get(), B.Texture.get());
        return std::less<UStaticMeshAsset*>{}(A.StaticMesh.get(), B.StaticMesh.get());
    });
    UpdateConstantBuffer(1, ViewProjection);
    for (const FRenderInfo& Info : Infos)
    {
        if (!Info.StaticMesh) continue;
        const UStaticMeshAsset& Mesh = *Info.StaticMesh;
        const auto Vertices = Mesh.GetVertexBuffer();
        const auto Indices = Mesh.GetIndexBuffer();
        if (!Vertices) continue;
        const bool HasTexture = static_cast<bool>(Info.Texture);
        // Preserve Week3's textured-white / untextured-vertex-color behavior.
        UpdateConstantBuffer(0, FMeshConstants{ Info.WorldTransformMatrix,
            FVector4(1, 1, 1, 1), HasTexture ? 0 : 1, HasTexture ? 1 : 0 });
        SetShaderResource(0, HasTexture ? Info.Texture->GetSRV().Get() : nullptr);
        DrawBuffers(Vertices.Get(), Mesh.GetVertexCount(), Indices.Get(), Indices ? Mesh.GetIndexCount() : 0);
    }
}
