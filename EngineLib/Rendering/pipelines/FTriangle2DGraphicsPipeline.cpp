#include "FTriangle2DGraphicsPipeline.h"
#include "../Renderer.h"

namespace
{
    struct FTriangle2DConstants { FMatrix Projection; FVector4 Color; FVector2 Center; float Size; float Rotation; };
}

FTriangle2DGraphicsPipeline::FTriangle2DGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_NONE);
    SetDepthStencilState(false, false);
    SetShader("Assets/Shaders/Triangle2D.hlsl");
    AddConstantBuffer<FTriangle2DConstants>();
}

void FTriangle2DGraphicsPipeline::Draw(TArray<FRenderInfo>& Infos, const FMatrix& Projection)
{
    BeginDraw();
    for (const FRenderInfo& Info : Infos)
    {
        UpdateConstantBuffer(0, FTriangle2DConstants{ Projection, Info.Color, Info.Center, Info.Size, Info.Rotation - PI * 0.5f });
        DrawProcedural(3);
    }
}
