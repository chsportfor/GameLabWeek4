#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

class FMeshGraphicsPipeline final : public FGraphicsPipeline
{
public:

    explicit FMeshGraphicsPipeline(URenderer& Renderer, bool ForceSolid = false);
    void SetTwoSided(bool bTwoSided);
    void Draw(TArray<FRenderStaticMeshInfo>& Infos, const FRenderView& View);
	void Draw(TArray<FRenderMeshInfo>& Infos, const FRenderView& View);
};
