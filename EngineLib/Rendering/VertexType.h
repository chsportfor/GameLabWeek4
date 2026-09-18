#pragma once

#include "Core/Core.h"
#include "Core/Math/Vector.h"

// 1. Define the triangle vertices
struct FVertexSimple
{
	float x, y, z;    // Position
	float nx, ny, nz;
	float r, g, b, a; // Color
	float u, v;       // Texture coordinates

	constexpr FVertexSimple() = default;
	constexpr FVertexSimple(float X, float Y, float Z, float R, float G, float B, float A,
		float U = 0, float V = 0) : x(X), y(Y), z(Z), nx(0), ny(0), nz(0), r(R), g(G), b(B), a(A), u(U), v(V) {}
	constexpr FVertexSimple(float X, float Y, float Z, float NX, float NY, float NZ,
		float R, float G, float B, float A, float U, float V)
		: x(X), y(Y), z(Z), nx(NX), ny(NY), nz(NZ), r(R), g(G), b(B), a(A), u(U), v(V) {}
	FVector GetPosition() const { return FVector(x, y, z); }
};
