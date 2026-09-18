#pragma once

#include "Rendering/VertexType.h"

// Baked mesh data: position, normal, color, UV. No runtime topology generation.
inline constexpr FVertexSimple Quad_vertices[] =
{
    { 0.0f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f },
    { 0.0f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f },
    { 0.0f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 0.0f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f },
    { 0.0f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f },
};

inline constexpr uint32 Quad_indices[] =
{
    0, 1, 2, 3, 4, 5,
};

inline constexpr FVertexSimple Fullscreen_vertices[] =
{
    { -1, 1, 0, 0, 0, -1, 1, 1, 1, 1, 0, 0 },
    { 1, 1, 0, 0, 0, -1, 1, 1, 1, 1, 1, 0 },
    { -1, -1, 0, 0, 0, -1, 1, 1, 1, 1, 0, 1 },
    { 1, -1, 0, 0, 0, -1, 1, 1, 1, 1, 1, 1 },
};
inline constexpr uint32 Fullscreen_indices[] = { 0, 1, 2, 2, 1, 3 };
