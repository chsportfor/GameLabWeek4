#pragma once
#include "Core/Math/Vector.h"

struct FLinearColor
{
	float R;
	float G;
	float B;
	float A;
	operator FVector4() const { return FVector4(R, G, B, A); }
};
