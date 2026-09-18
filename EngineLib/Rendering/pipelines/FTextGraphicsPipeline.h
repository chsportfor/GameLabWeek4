#pragma once
#include "../GraphicsPipeline.h"
class FTextGraphicsPipeline final : public FGraphicsPipeline
{
public:
    explicit FTextGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderTextInfo>& Infos, const FRenderView& View);
private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> Vertices;
    Microsoft::WRL::ComPtr<ID3D11Buffer> Indices;
};
