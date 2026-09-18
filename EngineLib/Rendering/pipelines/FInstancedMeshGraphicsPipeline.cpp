#include "FInstancedMeshGraphicsPipeline.h"
#include "Rendering/Renderer.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
namespace { struct FMeshInstance { FMatrix World; FLinearColor Tint; }; }
FInstancedMeshGraphicsPipeline::FInstancedMeshGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_BACK, 0, {EViewModeIndex::VMI_Lit, EViewModeIndex::VMI_Wireframe});
    SetDepthStencilState(true, true);
    SetShader("Assets/Shaders/InstancedMesh.hlsl", true);
    AddConstantBuffer<FMatrix>();
    Instances = Renderer.CreateStructuredBuffer<FMeshInstance>(BatchCapacity);
    SetShaderResource(1, Instances->SRV.Get());
}
void FInstancedMeshGraphicsPipeline::Draw(const TArray<FRenderMeshInfo>& Infos, const FRenderView& View)
{
    BeginDraw();
    UpdateConstantBuffer(0, View.ViewProjection);
    TMap<TSharedPtr<FStaticMeshAsset>, TArray<FMeshInstance>> Batches;
    for (const auto& Info : Infos)
        if (Info.StaticMesh) Batches[Info.StaticMesh].Add({Info.WorldTransformMatrix, Info.Color});
    for (const auto& [Mesh, Data] : Batches)
    {
        Bind();
        BindBuffers(Mesh->GetVertexBuffer().Get(), Mesh->GetIndexBuffer().Get());
        for (uint32 Offset = 0; Offset < Data.Num(); Offset += BatchCapacity)
        {
            const uint32 Count = FMath::Min(BatchCapacity, Data.Num() - Offset);
            Instances->UpdateStructuredBuffer(Data.GetData() + Offset, Count);
            Context->DrawIndexedInstanced(Mesh->GetIndexCount(), Count, 0, 0, 0);
        }
    }
}
