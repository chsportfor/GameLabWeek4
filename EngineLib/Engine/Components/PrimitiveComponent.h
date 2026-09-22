#pragma once

#include "Core/Math/Color.h"

#include <span>
#include "SceneComponent.h"

class UPrimitiveComponent : public USceneComponent
{
	DECLARE_OBJECT(UPrimitiveComponent, USceneComponent)
	DECLARE_SERIALIZATION()

public:
	UPrimitiveComponent() = default;
	virtual ~UPrimitiveComponent() = default;

	FBoundingBox CalcBounds(const FMatrix& LocalToWorld) const override { return mLocalBounds.ToWorld(LocalToWorld); }
	void RegisterPickTarget(FPickTargets& Targets) const override;
	// Components decide their own hit shape. HitT is comparable across transforms.
	virtual bool RayCastComponent(const FPickingRay& Ray, const FCamera& Camera, float& OutHitT) const;
	static std::span<const FPropertyInfo> GetDeclaredProperties();

protected:
	virtual FRenderMeshInfo MakeMeshInfo(const FRenderCollector& Collector) const;
	virtual FMatrix GetRenderTransform(const FCamera& Camera) const;
	bool BuildLocalPickingRay(const FPickingRay& Ray, const FCamera& Camera,
		FPickingRay& OutLocalRay) const;
	void SubmitSelection(FRenderCollector& Collector, const FMatrix& Model) const;
	FBoundingBox mLocalBounds{};

	bool mbShowBoundingBox = true;
};
