#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FWorldGridGraphicsPipeline final : public FGraphicsPipeline
{
public:
    using FRenderInfo = FRenderWorldGridInfo;

    explicit FWorldGridGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderInfo>& Infos, const FMatrix& ViewProjection, const FVector& CameraLocation);
};
