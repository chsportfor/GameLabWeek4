#include "FStencilOutlineGraphicsPipeline.h"
#include "../Renderer.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"

namespace
{
    struct FMeshConstants { FMatrix Model; FVector4 Color; int32 UseVertexColor; int32 HasTexture; int32 Padding[2]{}; };
}

FStencilOutlineGraphicsPipeline::FStencilOutlineGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_BACK);
    SetStencilState(false, false, D3D11_COMPARISON_NOT_EQUAL, D3D11_STENCIL_OP_KEEP, 1);
    SetShader("Assets/Shaders/Mesh.hlsl", true);
    AddConstantBuffer<FMeshConstants>();
    AddConstantBuffer<FMatrix>();
    SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);
}

void FStencilOutlineGraphicsPipeline::Draw(TArray<FRenderInfo>& Infos, const FMatrix& ViewProjection)
{
    BeginDraw();
    UpdateConstantBuffer(1, ViewProjection);
    for (const FRenderInfo& Info : Infos)
    {
        if (!Info.StaticMesh) continue;
        const UStaticMeshAsset& Mesh = *Info.StaticMesh;
        const auto Vertices = Mesh.GetVertexBuffer();
        const auto Indices = Mesh.GetIndexBuffer();
        if (!Vertices) continue;
        UpdateConstantBuffer(0, FMeshConstants{ Info.WorldTransformMatrix, Info.Color, 0, 0 });
        DrawBuffers(Vertices.Get(), Mesh.GetVertexCount(), Indices.Get(), Indices ? Mesh.GetIndexCount() : 0);
    }
}
