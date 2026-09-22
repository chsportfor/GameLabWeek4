#include "BillboardComponent.h"
#include "Rendering/Camera.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Rendering/BuiltinAssetNames.h"
#include "Rendering/Primitives/Primitives.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(UBillboardComponent, UPrimitiveComponent);
IMPLEMENT_SERIALIZATION(UBillboardComponent, UPrimitiveComponent, {})

UBillboardComponent::UBillboardComponent()
{
    mLocalBounds = FBoundingBox(FVector(0.f, -0.5f, -0.5f), FVector(0.f, 0.5f, 0.5f));
}

void UBillboardComponent::Initialize(FVector location, FRotator rotation, FVector scale3D)
{
	USceneComponent::Initialize(location, rotation, scale3D);
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
    Info.Texture = mTexture;
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

bool UBillboardComponent::RayCastComponent(const FPickingRay& Ray, const FCamera& Camera, float& OutHitT) const
{
    FPickingRay LocalRay;
    if (!MakeLocalPickingRay(Ray, GetRenderTransform(Camera), mLocalBounds, LocalRay)) return false;
    return RayCastTriangles(LocalRay, Quad_vertices, Quad_indices, OutHitT);
}

FRenderMeshInfo UBillboardComponent::MakeMeshInfo(const FRenderCollector& Collector) const
{
    FRenderMeshInfo Info{};
    Info.StaticMesh = Collector.AssetManager->GetAssetAs<UStaticMeshAsset>(BuiltinAssetNames::QuadMesh, true);
    Info.WorldTransformMatrix = GetRenderTransform(Collector.View.Camera);
    return Info;
}

std::span<const FPropertyInfo> UBillboardComponent::GetDeclaredProperties()
{
    static const FPropertyInfo Properties[] = {
        REFLECT_PROPERTY(UBillboardComponent, mTexture),
        REFLECT_PROPERTY(UBillboardComponent, mColor)
    };
    return Properties;
}
