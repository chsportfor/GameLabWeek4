#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FCircle2DGraphicsPipeline final : public FGraphicsPipeline
{
public:
    explicit FCircle2DGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderCircle2DInfo>& Infos, const FRenderView& View);
};
