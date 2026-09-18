#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FStencilMarkGraphicsPipeline final : public FGraphicsPipeline
{
public:
    using FRenderInfo = FRenderInfo;

    explicit FStencilMarkGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderInfo>& Infos, const FMatrix& ViewProjection);
};
