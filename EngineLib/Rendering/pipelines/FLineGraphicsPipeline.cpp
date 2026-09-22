#include "FLineGraphicsPipeline.h"
#include "../Renderer.h"

namespace
{
    struct FCameraConstants { FMatrix ViewProjection; FVector2 ViewportSize; float Padding[2]{}; };
}

FLineGraphicsPipeline::FLineGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    LineBuffer = Renderer.CreateStructuredBuffer<FRenderLineInfo>(MaxLineInstances);
    SetRasterizerState(D3D11_CULL_NONE);
    SetDepthStencilState(true, true);
    SetShader("Shaders/Line.hlsl");
    AddConstantBuffer<FCameraConstants>();
    SetShaderResource(0, LineBuffer->SRV.Get());
}

void FLineGraphicsPipeline::Draw(TArray<FRenderLineInfo>& Lines, const FRenderView& View)
{
    BeginDraw();
    if (Lines.IsEmpty()) return;
    UpdateConstantBuffer(0, FCameraConstants{ View.ViewProjection, View.ViewportSize });
    Bind();
    ID3D11Buffer* NoVertexBuffer = nullptr;
    UINT Zero = 0;
    Context->IASetVertexBuffers(0, 1, &NoVertexBuffer, &Zero, &Zero);
    Context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
    uint32 Remaining = Lines.Num();
    const FRenderLineInfo* Data = Lines.GetData();
    while (Remaining)
    {
        const uint32 Count = FGenericPlatformMath::Min(Remaining, MaxLineInstances);
        LineBuffer->UpdateStructuredBuffer(Data, Count);
        Context->DrawInstanced(6, Count, 0, 0);
        Remaining -= Count;
        Data += Count;
    }
}
