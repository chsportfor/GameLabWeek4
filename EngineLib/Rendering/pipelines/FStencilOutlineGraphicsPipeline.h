#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FStencilOutlineGraphicsPipeline final : public FGraphicsPipeline
{
public:
    using FRenderInfo = FRenderInfo;

    explicit FStencilOutlineGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderInfo>& Infos, const FMatrix& ViewProjection);
};
