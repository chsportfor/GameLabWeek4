#pragma once

#include "Core/Container/TArray.h"

#include "VertexType.h"

struct FSubUVMesh
{
	FVector2 UVScale{ 1.0f, 1.0f };
	FVector2 UVOffset{ 0.0f, 0.0f };

	FSubUVMesh() = default;
	FSubUVMesh(uint32 n, uint32 m);

	void UpdateMesh(uint32 n, uint32 m, uint32 i);

private:
	// Generate the mesh from i-th item of n by m grid
	void generateMesh(uint32 n, uint32 m, uint32 i);
};
