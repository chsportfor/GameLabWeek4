#pragma once

#include "Rendering/VertexType.h"

// Baked mesh data: position, normal, color, UV. No runtime topology generation.
inline constexpr FVertexSimple Triangle_vertices[] =
{
    { 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.5f, -0.5f },
    { 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.5f, 1.5f },
    { 0.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, -0.5f, 1.5f },
};

inline constexpr uint32 Triangle_indices[] =
{
    0, 1, 2,
};
