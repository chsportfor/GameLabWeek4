#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FLine2DGraphicsPipeline final : public FGraphicsPipeline
{
public:
    using FRenderInfo = FRenderLine2DInfo;

    explicit FLine2DGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderInfo>& Infos, const FMatrix& Projection);
};
