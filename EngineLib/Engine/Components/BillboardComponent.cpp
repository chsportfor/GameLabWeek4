#include "BillboardComponent.h"
#include "Rendering/Camera.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/Asset/FontAtlasAsset.h"
#include "Rendering/BuiltinAssetNames.h"

IMPLEMENT_CLASS(UBillboardComponent, UPrimitiveComponent);

UBillboardComponent::UBillboardComponent()
{
}

void UBillboardComponent::Initialize(FVector location, FRotator rotation, FVector scale3D)
{
	UPrimitiveComponent::Initialize(EPrimitive::EP_BillboardQuad, location, rotation, scale3D);
}

FMatrix UBillboardComponent::GetRenderTransform(const FCamera& Camera) const
{
    const auto World = GetTransformMatrix();
    const FVector Scale = World.GetScale();
    const FVector Axes[] = {Camera.GetForwardVector() * Scale.x,
        Camera.GetRightVector() * Scale.y, Camera.GetUpVector() * Scale.z};
    FMatrix Model = FMatrix::Translation(World.GetTranslation());
    for (int I = 0; I < 3; ++I)
    {
        Model.M[I][0] = Axes[I].x; Model.M[I][1] = Axes[I].y; Model.M[I][2] = Axes[I].z;
    }
    return Model;
}

FRenderQuadInfo UBillboardComponent::MakeQuadInfo(const FRenderCollector& Collector) const
{
    FRenderQuadInfo Info{};
    Info.Model = GetRenderTransform(Collector.View.Camera);
    Info.Color = mColor;
    Info.Texture = Collector.AssetManager->GetAssetAs<UTexture2D>(BuiltinAssetNames::Texture(mePrimitive), true);
    return Info;
}

void UBillboardComponent::SubmitRenderInfos(FRenderCollector& Collector) const
{
    const auto Model = GetRenderTransform(Collector.View.Camera);
    const auto Bounds = mLocalBounds.ToWorld(Model);
    SubmitSelection(Collector, Model);
    if (!Collector.IsVisible(Bounds)) return;
    if (Collector.HasShowFlag(EEngineShowFlags::SF_Primitives))
        Collector.QuadInfos.Add(MakeQuadInfo(Collector));
}
