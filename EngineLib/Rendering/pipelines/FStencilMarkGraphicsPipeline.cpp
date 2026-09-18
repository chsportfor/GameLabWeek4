#include "FStencilMarkGraphicsPipeline.h"
#include "../Renderer.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include <algorithm>
#include <functional>

#include "FMeshShaderConstants.h"

FStencilMarkGraphicsPipeline::FStencilMarkGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_BACK);
    SetStencilState(false, false, D3D11_COMPARISON_ALWAYS, D3D11_STENCIL_OP_REPLACE, 1);
    SetBlendState(ERenderBlendMode::NoColorWrite);
    SetShader("Assets/Shaders/Mesh.hlsl", true);
    AddConstantBuffer<FMeshShaderConstants>();
    AddConstantBuffer<FMatrix>();
    SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);
}

void FStencilMarkGraphicsPipeline::Draw(TArray<FRenderMeshInfo>& Infos, const FRenderView& View)
{
    BeginDraw();
    std::sort(Infos.begin(), Infos.end(), [](const FRenderMeshInfo& A, const FRenderMeshInfo& B)
    {
        return std::less<UStaticMeshAsset*>{}(A.StaticMesh.get(), B.StaticMesh.get());
    });
    UpdateConstantBuffer(1, View.ViewProjection);
    for (const FRenderMeshInfo& Info : Infos)
    {
        if (!Info.StaticMesh) continue;
        const UStaticMeshAsset& Mesh = *Info.StaticMesh;
        const auto Vertices = Mesh.GetVertexBuffer();
        const auto Indices = Mesh.GetIndexBuffer();
        if (!Vertices) continue;
        UpdateConstantBuffer(0, FMeshShaderConstants{ Info.WorldTransformMatrix, FVector4(1, 1, 1, 1), 0, 0 });
        DrawBuffers(Vertices.Get(), Mesh.GetVertexCount(), Indices.Get(), Indices ? Mesh.GetIndexCount() : 0);
    }
}
