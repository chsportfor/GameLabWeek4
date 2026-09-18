#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FMeshGraphicsPipeline final : public FGraphicsPipeline
{
public:
    using FRenderInfo = FRenderInfo;

    explicit FMeshGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderInfo>& Infos, const FMatrix& ViewProjection);
};
