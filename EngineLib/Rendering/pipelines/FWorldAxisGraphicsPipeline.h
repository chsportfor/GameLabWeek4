#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FWorldAxisGraphicsPipeline final : public FGraphicsPipeline
{
public:
    using FRenderInfo = FRenderWorldAxisInfo;

    explicit FWorldAxisGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderInfo>& Infos, const FMatrix& View,
        const FMatrix& Projection, const FVector2& ViewportSize);
};
