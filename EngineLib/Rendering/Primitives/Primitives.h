#pragma once

#include "Rendering/VertexType.h"

inline FVertexSimple Quad_vertices[6] =
{
	// YZ plane, X = 0, left-handed, clockwise winding order
	{ 0.0f, -0.5f, +0.5f,  1.0f, 0.0f, 0.0f, 1.0f },   // left-top triangle
	{ 0.0f, +0.5f, +0.5f,   0.0f, 1.0f, 0.0f, 1.0f },
	{ 0.0f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f },
	{ 0.0f, +0.5f, -0.5f,  1.0f, 1.0f, 1.0f, 1.0f },   // right-bottom triangle
	{ 0.0f, -0.5f, -0.5f,   1.0f, 1.0f, 1.0f, 1.0f },
	{ 0.0f, +0.5f, +0.5f,    1.0f, 1.0f, 1.0f, 1.0f },
};

inline FVertexTextured Quad_textured_vertices[6] =
{
	// YZ plane, X = 0, left-handed, clockwise winding order
	{ 0.0f, -0.5f, +0.5f,  0.0f, 0.0f },   // left-top triangle
	{ 0.0f, +0.5f, +0.5f,   1.0f, 0.0f },
	{ 0.0f, -0.5f, -0.5f,  0.0f, 1.0f },
	{ 0.0f, +0.5f, -0.5f,  1.0f, 1.0f },   // right-bottom triangle
	{ 0.0f, -0.5f, -0.5f,   0.0f, 1.0f },
	{ 0.0f, +0.5f, +0.5f,    1.0f, 0.0f },
};
