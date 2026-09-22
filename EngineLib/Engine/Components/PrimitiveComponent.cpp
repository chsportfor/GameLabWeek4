#include "PrimitiveComponent.h"
#include "Engine/Actor.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(UPrimitiveComponent, USceneComponent);
IMPLEMENT_SERIALIZATION(UPrimitiveComponent, USceneComponent, {})

FMatrix UPrimitiveComponent::GetRenderTransform(const FCamera&) const
{
    return GetTransformMatrix();
}

void UPrimitiveComponent::RegisterPickTarget(FPickTargets& Targets) const
{
    Targets.Add(this);
}

bool UPrimitiveComponent::RayCastComponent(const FPickingRay&, const FCamera&, float&) const
{
    return false;
}

bool UPrimitiveComponent::BuildLocalPickingRay(const FPickingRay& Ray, const FCamera& Camera,
	FPickingRay& OutLocalRay) const
{
	const FMatrix renderTransform = GetRenderTransform(Camera);
	if (renderTransform != GetTransformMatrix())
	{
		return MakeLocalPickingRay(Ray, renderTransform, mLocalBounds, OutLocalRay);
	}

	if (!RayIntersectsBounds(Ray, mLocalBounds.ToWorld(renderTransform))) return false;
	OutLocalRay = {
		InverseTransformPosition(Ray.Near),
		InverseTransformPosition(Ray.Far)
	};
	return RayIntersectsBounds(OutLocalRay, mLocalBounds);
}

FRenderMeshInfo UPrimitiveComponent::MakeMeshInfo(const FRenderCollector& Collector) const
{
    return {};
}

void UPrimitiveComponent::SubmitSelection(FRenderCollector& Collector, const FMatrix& Model) const
{
    if (!mOwner || mOwner != Collector.SelectedActor) return;
    auto Info = MakeMeshInfo(Collector);
    Info.WorldTransformMatrix = Model;
    if (Info.StaticMesh) Collector.SelectionInfos.Add(Info);
    if (mbShowBoundingBox && Collector.HasShowFlag(EEngineShowFlags::SF_BoundingBox))
    {
        const auto Bounds = mLocalBounds.ToWorld(Model);
        if (Collector.IsVisible(Bounds)) Collector.AddBounds(Bounds);
    }
}

std::span<const FPropertyInfo> UPrimitiveComponent::GetDeclaredProperties()
{
    static const FPropertyInfo Properties[] = {
        REFLECT_PROPERTY(UPrimitiveComponent, mbShowBoundingBox)
    };
    return Properties;
}
