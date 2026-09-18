#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FWorldGridGraphicsPipeline final : public FGraphicsPipeline
{
public:
    explicit FWorldGridGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderWorldGridInfo>& Infos, const FRenderView& View);
};
