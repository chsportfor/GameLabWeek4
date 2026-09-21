#pragma once

#include "Core/Math/Color.h"

#include <span>
#include "SceneComponent.h"

class UPrimitiveComponent : public USceneComponent
{
	DECLARE_OBJECT(UPrimitiveComponent, USceneComponent)
	DECLARE_SERIALIZATION()

public:
	UPrimitiveComponent();

	void Initialize(EPrimitive ePrimitive);
	void Initialize(EPrimitive ePrimitive, FVector location, FRotator rotation, FVector scale3D);
	void Initialize(EPrimitive ePrimitive, FVector location, FRotator rotation, FVector scale3D, bool bUseTexture);

	virtual ~UPrimitiveComponent();

	FBoundingBox CalcBounds(const FMatrix& LocalToWorld) const override { return mLocalBounds.ToWorld(LocalToWorld); }
	void SubmitRenderInfos(FRenderCollector& Collector) const override;
	void RegisterPickTarget(FPickTargets& Targets) const override;
	// Components decide their own hit shape. HitT is comparable across transforms.
	virtual bool RayCastComponent(const FPickingRay& Ray, const FCamera& Camera, float& OutHitT) const;
	void SetUseTexture(bool value) { mbUseTexture = value; }
	bool GetUseTexture() const { return mbUseTexture; }

	const FLinearColor& GetColor() const { return mColor; }
	void SetColor(const FLinearColor& color) { mColor = color; }

	static std::span<const FPropertyInfo> GetDeclaredProperties();

protected:
	virtual FRenderMeshInfo MakeMeshInfo(const FRenderCollector& Collector) const;
	virtual FMatrix GetRenderTransform(const FCamera& Camera) const;
	void SubmitSelection(FRenderCollector& Collector, const FMatrix& Model) const;

	EPrimitive mePrimitive = EPrimitive::EP_Cube;
	FLinearColor mColor{ 1.f, 1.f, 1.f, 1.f };

	FBoundingBox mLocalBounds{};

	bool mbUseTexture = false;
	bool mbShowBoundingBox = true;
};


