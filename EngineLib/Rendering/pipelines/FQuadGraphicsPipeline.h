#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FQuadGraphicsPipeline final : public FGraphicsPipeline
{
public:
    using FRenderInfo = FRenderQuadInfo;

    explicit FQuadGraphicsPipeline(URenderer& Renderer);
    // Sorts opaque state groups; consumes transparent/overlay with swap-and-pop.
    void Draw(TArray<FRenderInfo>& Infos, EQuadRenderPhase Phase, const FMatrix& ViewProjection);
    static EQuadRenderPhase GetPhase(const FRenderInfo& Info);
};
