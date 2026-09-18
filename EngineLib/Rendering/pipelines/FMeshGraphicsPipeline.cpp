#include "FMeshGraphicsPipeline.h"
#include "Core/AssetSystem/Asset/Texture2DAsset.h"
#include "../Renderer.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include <algorithm>
#include <functional>

#include "FMeshShaderConstants.h"

FMeshGraphicsPipeline::FMeshGraphicsPipeline(URenderer& Renderer, bool ForceSolid) : FGraphicsPipeline(Renderer)
{
    if (ForceSolid) SetRasterizerState(D3D11_CULL_BACK);
    else SetRasterizerState(D3D11_CULL_BACK, 0, { EViewModeIndex::VMI_Lit, EViewModeIndex::VMI_Wireframe });
    SetDepthStencilState(true, true);
    SetShader("Assets/Shaders/Mesh.hlsl", true);
    AddConstantBuffer<FMeshShaderConstants>();
    AddConstantBuffer<FMatrix>();
    SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);
}

void FMeshGraphicsPipeline::Draw(TArray<FRenderMeshInfo>& Infos, const FRenderView& View)
{
    BeginDraw();
    std::sort(Infos.begin(), Infos.end(), [](const FRenderMeshInfo& A, const FRenderMeshInfo& B)
    {
        if (A.Texture.get() != B.Texture.get())
            return std::less<FTexture2DAsset*>{}(A.Texture.get(), B.Texture.get());
        return std::less<FStaticMeshAsset*>{}(A.StaticMesh.get(), B.StaticMesh.get());
    });
    UpdateConstantBuffer(1, View.ViewProjection);
    for (const FRenderMeshInfo& Info : Infos)
    {
        if (!Info.StaticMesh) continue;
        const FStaticMeshAsset& Mesh = *Info.StaticMesh;
        const auto Vertices = Mesh.GetVertexBuffer();
        const auto Indices = Mesh.GetIndexBuffer();
        if (!Vertices) continue;
        const uint32 IndexCount = Info.IndexCount ? Info.IndexCount : Mesh.GetIndexCount();
        if (Indices && (Info.FirstIndex > Mesh.GetIndexCount() || IndexCount > Mesh.GetIndexCount() - Info.FirstIndex)) continue;
        const bool HasTexture = static_cast<bool>(Info.Texture);
        // Preserve the application tint blend and texture atlas transform.
        UpdateConstantBuffer(0, FMeshShaderConstants{ Info.WorldTransformMatrix,
            Info.Color, HasTexture ? 0 : 1, HasTexture ? 1 : 0, {},
            Info.UVScale,
            Info.UVOffset });
        SetShaderResource(0, HasTexture ? Info.Texture->GetSRV().Get() : nullptr);
        DrawBuffers(Vertices.Get(), Mesh.GetVertexCount(), Indices.Get(), Indices ? IndexCount : 0, Info.FirstIndex);
    }
}
