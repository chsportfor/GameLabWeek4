#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FLine2DGraphicsPipeline final : public FGraphicsPipeline
{
public:
    explicit FLine2DGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderLine2DInfo>& Infos, const FRenderView& View);
};
