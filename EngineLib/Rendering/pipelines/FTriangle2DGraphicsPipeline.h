#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FTriangle2DGraphicsPipeline final : public FGraphicsPipeline
{
public:
    explicit FTriangle2DGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderTriangle2DInfo>& Infos, const FRenderView& View);
};
