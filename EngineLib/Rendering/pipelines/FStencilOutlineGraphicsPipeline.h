#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FStencilOutlineGraphicsPipeline final : public FGraphicsPipeline
{
public:

    explicit FStencilOutlineGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderMeshInfo>& Infos, const FRenderView& View);
};
