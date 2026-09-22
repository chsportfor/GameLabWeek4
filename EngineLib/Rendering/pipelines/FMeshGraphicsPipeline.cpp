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
    else SetTwoSided(false);
    SetDepthStencilState(true, true);
    SetShader("Assets/Shaders/Mesh.hlsl", true);
    AddConstantBuffer<FMeshShaderConstants>();
    AddConstantBuffer<FMatrix>();
    SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);
}

void FMeshGraphicsPipeline::SetTwoSided(bool bTwoSided)
{
    SetRasterizerState(bTwoSided ? D3D11_CULL_NONE : D3D11_CULL_BACK, 0,
        { EViewModeIndex::VMI_Lit, EViewModeIndex::VMI_Wireframe });
}

void FMeshGraphicsPipeline::Draw(TArray<FRenderMeshInfo>& Infos, const FRenderView& View)
{
    BeginDraw();
    std::sort(Infos.begin(), Infos.end(), [](const FRenderMeshInfo& A, const FRenderMeshInfo& B)
    {
        if (A.Texture != B.Texture)
            return std::less<UTexture2D*>{}(A.Texture, B.Texture);
        return std::less<UStaticMeshAsset*>{}(A.StaticMesh, B.StaticMesh);
    });
    UpdateConstantBuffer(1, View.ViewProjection);
    for (const FRenderMeshInfo& Info : Infos)
    {
        if (!Info.StaticMesh) continue;
        const UStaticMeshAsset& Mesh = *Info.StaticMesh;
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


void FMeshGraphicsPipeline::Draw(TArray<FRenderStaticMeshInfo>& Infos, const FRenderView& View)
{
	BeginDraw();
	UpdateConstantBuffer(1, View.ViewProjection);
	for (const FRenderStaticMeshInfo& Info : Infos)
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> Vertices = Info.VertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> Indices = Info.IndexBuffer;
		if (!Vertices) continue;
		const uint32 IndexCount = Info.IndexCount ? Info.IndexCount : 0;
		const bool HasTexture = static_cast<bool>(Info.Texture);
		// Preserve the application tint blend and texture atlas transform.
		UpdateConstantBuffer(0, FMeshShaderConstants{ Info.WorldTransformMatrix,Info.Color, HasTexture ? 0 : 1,
		HasTexture ? 1 : 0,{}, Info.UVScale,Info.UVOffset });
		SetShaderResource(0, HasTexture ? Info.Texture->GetSRV().Get() : nullptr);
		DrawBuffers(Vertices.Get(), Info.VertexCount, Indices.Get(), Indices ? IndexCount : 0, Info.FirstIndex);
	}
}
