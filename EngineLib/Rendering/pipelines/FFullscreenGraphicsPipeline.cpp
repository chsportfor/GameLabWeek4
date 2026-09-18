#include "FFullscreenGraphicsPipeline.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/Asset/Texture2DAsset.h"
FFullscreenGraphicsPipeline::FFullscreenGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_NONE);
    SetDepthStencilState(false, false);
    SetShader("Assets/Shaders/Fullscreen.hlsl", true);
    SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP);
}
void FFullscreenGraphicsPipeline::Draw(TArray<FRenderFullscreenInfo>& Infos)
{
    BeginDraw();
    for (const auto& Info : Infos)
    {
        if (!Info.StaticMesh || !Info.Texture) continue;
        const auto& Mesh = *Info.StaticMesh;
        SetShaderResource(0, Info.Texture->GetSRV().Get());
        DrawBuffers(Mesh.GetVertexBuffer().Get(), Mesh.GetVertexCount(), Mesh.GetIndexBuffer().Get(), Mesh.GetIndexCount());
    }
    SetShaderResource(0, nullptr);
}
