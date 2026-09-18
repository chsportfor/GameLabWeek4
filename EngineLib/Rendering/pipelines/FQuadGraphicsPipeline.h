#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FQuadGraphicsPipeline final : public FGraphicsPipeline
{
public:
    explicit FQuadGraphicsPipeline(URenderer& Renderer);
    // Consumes the requested phase; preserves transparent/overlay submission order.
    void Draw(TArray<FRenderQuadInfo>& Infos, const FRenderView& View, EQuadRenderPhase Phase);
    static EQuadRenderPhase GetPhase(const FRenderQuadInfo& Info);
};
