#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FWorldAxisGraphicsPipeline final : public FGraphicsPipeline
{
public:
    explicit FWorldAxisGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderWorldAxisInfo>& Infos, const FRenderView& View);
};
