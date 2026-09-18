#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

struct FStructuredBuffer;

class FLineGraphicsPipeline final : public FGraphicsPipeline
{
public:
    explicit FLineGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderLineInfo>& Lines, const FRenderView& View);
private:
    static constexpr uint32 MaxLineInstances = 1024;
    TSharedPtr<FStructuredBuffer> LineBuffer;
};
