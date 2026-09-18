#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FCircle2DGraphicsPipeline final : public FGraphicsPipeline
{
public:
    using FRenderInfo = FRenderCircle2DInfo;

    explicit FCircle2DGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderInfo>& Infos, const FMatrix& Projection);
};
