#pragma once

#include "../GraphicsPipeline.h"
#include "../RenderInfo.h"

struct FStructuredBuffer;

class FLineGraphicsPipeline final : public FGraphicsPipeline
{
public:
    using FRenderInfo = FRenderLineInfo;

    explicit FLineGraphicsPipeline(URenderer& Renderer);
    void Draw(TArray<FRenderInfo>& Lines, const FMatrix& ViewProjection, const FVector2& ViewportSize);
private:
    static constexpr uint32 MaxLineInstances = 1024;
    TSharedPtr<FStructuredBuffer> LineBuffer;
};
