#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FTriangle2DGraphicsPipeline final : public FGraphicsPipeline
{
public:
    using FRenderInfo = FRenderTriangle2DInfo;

    explicit FTriangle2DGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderInfo>& Infos, const FMatrix& Projection);
};
