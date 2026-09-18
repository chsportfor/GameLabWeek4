#pragma once
#include "../GraphicsPipeline.h"
struct FStructuredBuffer;
class FInstancedMeshGraphicsPipeline final : public FGraphicsPipeline
{
public:
    explicit FInstancedMeshGraphicsPipeline(URenderer& Renderer);
    void Draw(const TArray<FRenderMeshInfo>& Infos, const FRenderView& View);
private:
    TSharedPtr<FStructuredBuffer> Instances;
    static constexpr uint32 BatchCapacity = 1024;
};
