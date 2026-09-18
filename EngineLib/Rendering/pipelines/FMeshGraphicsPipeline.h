#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FMeshGraphicsPipeline final : public FGraphicsPipeline
{
public:

    explicit FMeshGraphicsPipeline(URenderer& Renderer, bool ForceSolid = false);
    void Draw(TArray<FRenderMeshInfo>& Infos, const FRenderView& View);
};
