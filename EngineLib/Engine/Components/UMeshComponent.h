#pragma once

#include "PrimitiveComponent.h"
#include "Core/Object/ObjectFactory.h"

class UMeshComponent : public UPrimitiveComponent
{
	DECLARE_OBJECT(UMeshComponent,UPrimitiveComponent)
	DECLARE_SERIALIZATION()
};
