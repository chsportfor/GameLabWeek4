#pragma once
#include "../GraphicsPipeline.h"
struct FStructuredBuffer;
class FFullscreenGraphicsPipeline final : public FGraphicsPipeline
{
public:
    explicit FFullscreenGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderFullscreenInfo>& Infos);
private:
    
};
