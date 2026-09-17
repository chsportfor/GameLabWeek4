#include "SubUVMesh.h"

FSubUVMesh::FSubUVMesh(uint32 n, uint32 m)
{
	generateMesh(n, m, 0);
}

void FSubUVMesh::UpdateMesh(uint32 n, uint32 m, uint32 i)
{
	generateMesh(n, m, i);
}

void FSubUVMesh::generateMesh(uint32 n, uint32 m, uint32 i)
{
	//const float width = 1.0f / m;
	//const float height = 1.0f / n;

	//const uint32 row = i / m;
	//const uint32 col = i - row * m;

	UVScale.x = 1.0f / m;
	UVScale.y = 1.0f / n;

	UVOffset.x = (i % m) * UVScale.x;
	UVOffset.y = (i / m) * UVScale.y;

	//const float u0 = row * height;
	//const float u1 = u0 + height;
	//const float v0 = col * width;
	//const float v1 = v0 + width;

	//// Add vertices for the quad
	//Vertices.Add({ 0.0f, -0.5f, +0.5f, v0, u0 }); // Top-left
	//Vertices.Add({ 0.0f, +0.5f, +0.5f, v1, u0 }); // Top-right
	//Vertices.Add({ 0.0f, +0.5f, -0.5f, v1, u1 }); // Bottom-right
	//Vertices.Add({ 0.0f, -0.5f, -0.5f, v0, u1 }); // Bottom-left

	//// Add indices for the quad
	//Indices.Add(0);
	//Indices.Add(1);
	//Indices.Add(2);
	//Indices.Add(0);
	//Indices.Add(2);
	//Indices.Add(3);
}

