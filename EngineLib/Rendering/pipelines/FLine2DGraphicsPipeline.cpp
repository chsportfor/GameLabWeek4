#include "FLine2DGraphicsPipeline.h"
#include "../Renderer.h"

namespace
{
    struct FLine2DConstants { FMatrix Projection; FVector4 Color; FVector2 Start; FVector2 End; float Thickness; float Padding[3]{}; };
}

FLine2DGraphicsPipeline::FLine2DGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_NONE);
    SetDepthStencilState(false, false);
    SetShader("Assets/Shaders/Line2D.hlsl");
    AddConstantBuffer<FLine2DConstants>();
}

void FLine2DGraphicsPipeline::Draw(TArray<FRenderInfo>& Infos, const FMatrix& Projection)
{
    BeginDraw();
    for (const FRenderInfo& Info : Infos)
    {
        UpdateConstantBuffer(0, FLine2DConstants{ Projection, Info.Color, Info.Start, Info.End, Info.Thickness });
        DrawProcedural(6);
    }
}
