#pragma once

#include "Core/Core.h"
#include "Core/Math/Vector.h"

struct FVertexSimple
{
	float x, y, z;    // Position
	float r, g, b, a; // Color

	FVector GetPosition() const { return FVector(x, y, z); }
};

struct FVertexTextured
{
	float x, y, z;
	float u, v;
};
