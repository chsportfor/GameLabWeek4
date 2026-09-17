#pragma once

#include "PrimitiveComponent.h"

class UCubeComponent : public UPrimitiveComponent
{
	DECLARE_OBJECT(UCubeComponent, UPrimitiveComponent)
public:
	UCubeComponent();

	/*
	void Initialize(GraphicsManager* graphicsManager);
	void Initialize(GraphicsManager* graphicsManager, FVector location, FRotator rotation, FVector scale3D);
	*/

	void Initialize();
	void Initialize(FVector location, FRotator rotation, FVector scale3D);

	virtual ~UCubeComponent();

};
