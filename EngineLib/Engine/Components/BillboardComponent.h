#pragma once

#include "PrimitiveComponent.h"

// Billboard rendered texture 2d quad component, always facing the **Viewport camera**.
class UBillboardComponent : public UPrimitiveComponent
{
	DECLARE_OBJECT(UBillboardComponent, UPrimitiveComponent)
public:
	UBillboardComponent();

	void Initialize(FVector location, FRotator rotation, FVector scale3D);

	virtual ~UBillboardComponent() = default;

protected:
	// mePrimitive = EPrimitive::BillboardQuad;

	// TODO: Add a texture to render on the billboard quad.
	// UTexture2D* mTexture;

	virtual FRenderInfo makeRenderInfo() const override;
};
