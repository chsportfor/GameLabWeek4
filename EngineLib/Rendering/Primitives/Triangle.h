#pragma once

#include "Rendering/VertexType.h"

inline FVertexSimple Triangle_vertices[3] = {
	{ 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f }, // Top vertex (red)
	{ 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f }, // Bottom left vertex (green)
	{ 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f } // Bottom right vertex (blue)
};
