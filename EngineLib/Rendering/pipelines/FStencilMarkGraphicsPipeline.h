#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FStencilMarkGraphicsPipeline final : public FGraphicsPipeline
{
public:

    explicit FStencilMarkGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderMeshInfo>& Infos, const FRenderView& View);
};
