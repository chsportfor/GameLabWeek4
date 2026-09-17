// TexturedPrimitives.h
#pragma once

#include "Rendering/VertexType.h" // FVertexTextured 정의
#include <cmath>

inline FVertexTextured QuadTextureVertices[] =
{
	// x      y      z       u     v
	{ -0.5f, -0.5f, 0.0f,   0.0f, 1.0f },
	{ -0.5f,  0.5f, 0.0f,   0.0f, 0.0f },
	{ 0.5f,  0.5f, 0.0f,   1.0f, 0.0f },

	{ -0.5f, -0.5f, 0.0f,   0.0f, 1.0f },
	{ 0.5f,  0.5f, 0.0f,   1.0f, 0.0f },
	{ 0.5f, -0.5f, 0.0f,   1.0f, 1.0f },
};

inline FVertexTextured QuadTextureIndexedVertices[] =
{
	// x      y      z       u     v
	{ 0.0f, -0.5f, +0.5f,   0.0f, 0.0f }, // left-top
	{ 0.0f, +0.5f, +0.5f,   1.0f, 0.0f }, // right-top
	{ 0.0f, +0.5f, -0.5f,   1.0f, 1.0f }, // right-bottom
	{ 0.0f, -0.5f, -0.5f,   0.0f, 1.0f }, // left-bottom
};

inline uint32 QuadTextureIndices[] =
{
	0, 1, 2, // right-top triangle
	0, 2, 3  // left-bottom triangle
};

// 삼각형: 정점 3개
inline FVertexTextured TriangleTextureVertices[] =
{
	// x      y      z       u     v
	{ -0.5f, -0.5f, 0.0f,   0.0f, 1.0f },
	{ 0.0f,  0.5f, 0.0f,   0.5f, 0.0f },
	{ 0.5f, -0.5f, 0.0f,   1.0f, 1.0f },
};

// 중심 (0, 0, 0), 한 변의 길이 1
inline FVertexTextured CubeTextureVertices[] =
{
	// x      y      z       u     v

	// -Z 면
	{ -0.5f, -0.5f, -0.5f,  0.0f, 1.0f },
	{ -0.5f,  0.5f, -0.5f,  0.0f, 0.0f },
	{ 0.5f,  0.5f, -0.5f,  1.0f, 0.0f },

	{ -0.5f, -0.5f, -0.5f,  0.0f, 1.0f },
	{ 0.5f,  0.5f, -0.5f,  1.0f, 0.0f },
	{ 0.5f, -0.5f, -0.5f,  1.0f, 1.0f },

	// +Z 면
	{ 0.5f, -0.5f,  0.5f,  0.0f, 1.0f },
	{ 0.5f,  0.5f,  0.5f,  0.0f, 0.0f },
	{ -0.5f,  0.5f,  0.5f,  1.0f, 0.0f },

	{ 0.5f, -0.5f,  0.5f,  0.0f, 1.0f },
	{ -0.5f,  0.5f,  0.5f,  1.0f, 0.0f },
	{ -0.5f, -0.5f,  0.5f,  1.0f, 1.0f },

	// -X 면
	{ -0.5f, -0.5f,  0.5f,  0.0f, 1.0f },
	{ -0.5f,  0.5f,  0.5f,  0.0f, 0.0f },
	{ -0.5f,  0.5f, -0.5f,  1.0f, 0.0f },

	{ -0.5f, -0.5f,  0.5f,  0.0f, 1.0f },
	{ -0.5f,  0.5f, -0.5f,  1.0f, 0.0f },
	{ -0.5f, -0.5f, -0.5f,  1.0f, 1.0f },

	// +X 면
	{ 0.5f, -0.5f, -0.5f,  0.0f, 1.0f },
	{ 0.5f,  0.5f, -0.5f,  0.0f, 0.0f },
	{ 0.5f,  0.5f,  0.5f,  1.0f, 0.0f },

	{ 0.5f, -0.5f, -0.5f,  0.0f, 1.0f },
	{ 0.5f,  0.5f,  0.5f,  1.0f, 0.0f },
	{ 0.5f, -0.5f,  0.5f,  1.0f, 1.0f },

	// +Y 면
	{ -0.5f,  0.5f, -0.5f,  0.0f, 1.0f },
	{ -0.5f,  0.5f,  0.5f,  0.0f, 0.0f },
	{ 0.5f,  0.5f,  0.5f,  1.0f, 0.0f },

	{ -0.5f,  0.5f, -0.5f,  0.0f, 1.0f },
	{ 0.5f,  0.5f,  0.5f,  1.0f, 0.0f },
	{ 0.5f,  0.5f, -0.5f,  1.0f, 1.0f },

	// -Y 면
	{ -0.5f, -0.5f,  0.5f,  0.0f, 1.0f },
	{ -0.5f, -0.5f, -0.5f,  0.0f, 0.0f },
	{ 0.5f, -0.5f, -0.5f,  1.0f, 0.0f },

	{ -0.5f, -0.5f,  0.5f,  0.0f, 1.0f },
	{ 0.5f, -0.5f, -0.5f,  1.0f, 0.0f },
	{ 0.5f, -0.5f,  0.5f,  1.0f, 1.0f },
};

// 6개의 면을 그리는 index
inline unsigned int CubeTextureIndices[36] =
{
	 0,  1,  2,     0,  2,  3, // -Z
	 4,  5,  6,     4,  6,  7, // +Z
	 8,  9, 10,     8, 10, 11, // -X
	12, 13, 14,    12, 14, 15, // +X
	16, 17, 18,    16, 18, 19, // +Y
	20, 21, 22,    20, 22, 23, // -Y
};



// 한 텍스쳐를 꽉 채운 6칸으로 생성할 때
inline void BuildCubeAtlasVertices(FVertexTextured(&outVertices)[36])
{
	// 기존 면 순서: -Z, +Z, -X, +X, +Y, -Y
	// 각 면에 배정할 아틀라스 칸 번호
/*	const int faceCells[6] = { 0, 1, 2, 3, 4, 5 };a

	for (int face = 0; face < 6; ++face)
	{
		const int cellIndex = faceCells[face];
		const int column = cellIndex % 3;
		const int row = cellIndex / 3;

		for (int vertex = 0; vertex < 6; ++vertex)
		{
			const int index = face * 6 + vertex;

			// 위치와 기존 UV 복사
			outVertices[index] = CubeTextureVertices[index];

			// UV만 선택한 칸 안으로 변환
			outVertices[index].u =	(column + CubeTextureVertices[index].u) / 3.0f;

			outVertices[index].v = (row + CubeTextureVertices[index].v) / 2.0f;
		}
	}*/

	const int faceCells[6] = { 5, 4, 3, 0, 2, 1 };

	for (int face = 0; face < 6; ++face)
	{
		const int column = faceCells[face] % 3;
		const int row = faceCells[face] / 3;

		for (int vertex = 0; vertex < 6; ++vertex)
		{
			const int index = face * 6 + vertex;
			const FVertexTextured& source =
				CubeTextureVertices[index];

			outVertices[index] = source;

			float u = source.u;
			float v = source.v;

			switch (face)
			{
			case 0: // -Z: Bottom
				u = source.v;
				v = 1.0f - source.u;
				break;

			case 1: // +Z: Top
				u = 1.0f - source.v;
				v = source.u;
				break;

			case 2: // -X: Back — 기존 유지
				u = 1.0f - source.v;
				v = source.u;
				break;

			case 3: // +X: Front — 기존 유지
				u = source.v;
				v = 1.0f - source.u;
				break;

			case 5: // -Y: Left — 기존 유지
				u = 1.0f - source.u;
				v = 1.0f - source.v;
				break;

				// +Y: Right — 기존 유지
			}

			outVertices[index].u = (column + u) / 3.0f;
			outVertices[index].v = (row + v) / 2.0f;
		}
	}
}

// sample과 동일하게 인덱스로 큐브 면을 나타낼때
inline void BuildCubeAtlasVertices(FVertexTextured(&outVertices)[36], int columns, int rows, const int(&faceCells)[6])
{
	for (int face = 0; face < 6; ++face)
	{
		const int cellIndex = faceCells[face];

		assert(cellIndex >= 0);
		assert(cellIndex / columns < rows);

		const int column = cellIndex % columns;
		const int row = cellIndex / columns;

		for (int vertex = 0; vertex < 6; ++vertex)
		{
			const int index = face * 6 + vertex;
			const FVertexTextured& source = CubeTextureVertices[index];

			// 정점 위치와 원본 UV 복사
			outVertices[index] = source;

			float u = source.u;
			float v = source.v;

			// face별 그림 방향 유지
			switch (face)
			{
			case 0: // -Z
				u = source.v;
				v = 1.0f - source.u;
				break;

			case 1: // +Z
			case 2: // -X
				u = 1.0f - source.v;
				v = source.u;
				break;

			case 3: // +X
				u = source.v;
				v = 1.0f - source.u;
				break;

			case 4: // +Y
				break;

			case 5: // -Y
				u = 1.0f - source.u;
				v = 1.0f - source.v;
				break;
			}

			// 면 내부의 0~1 UV를 선택한 칸으로 변환
			outVertices[index].u =	(column + u) / static_cast<float>(columns);

			outVertices[index].v =	(row + v) / static_cast<float>(rows);
		}
	}
}


// sphere uv 매핑(y축을 중심)
template <std::size_t N>
inline void BuildSphereTextureVertices(const FVertexSimple(&source)[N], TArray<FVertexTextured>& outVertices)
{
	static_assert(N % 3 == 0, "정점 개수는 3의 배수여야 합니다.");

	outVertices.Init(FVertexTextured{}, static_cast<uint32>(N));

	constexpr float pi = 3.14159265358979323846f;
	// 부동 소수점 처리
	constexpr float poleEpsilon = 0.00001f;

	// 삼각형 하나씩 처리
	for (std::size_t i = 0; i < N; i += 3)
	{
		bool isPole[3] = {};

		// 위치 복사 및 기본 UV 계산
		for (int j = 0; j < 3; ++j)
		{
			const FVertexSimple& src = source[i + j];
			FVertexTextured& dst = outVertices[i + j];

			dst.x = src.x;
			dst.y = src.y;
			dst.z = src.z;

			const float length = std::sqrt(	src.x * src.x + src.y * src.y +	src.z * src.z);

			// 정상적인 구 표면에는 원점 정점이 없음
			if (length == 0.0f)
			{
				dst.u = 0.0f;
				dst.v = 0.0f;
				continue;
			}

			// 벡터 정규화
			const float nx = src.x / length;
			float ny = src.y / length;
			const float nz = src.z / length;

			// acos 입력 범위를 부동소수점 오차로부터 보호
			if (ny > 1.0f) ny = 1.0f;
			if (ny < -1.0f) ny = -1.0f;

			// x= 0, z = 0인지 확인(북극과 남극)
			isPole[j] =	nx * nx + nz * nz < poleEpsilon * poleEpsilon;


			// 극점의 u는 아래에서 따로 계산
			dst.u = isPole[j] ? 0.0f : 0.5f + std::atan2(nz, nx) / (2.0f * pi);

			// acos의 범위: 0~ pi
			dst.v = std::acos(ny) / pi;
		}

		// 극점을 제외하고 이음새를 넘는지 확인
		float minU = 1.0f;
		float maxU = 0.0f;
		int nonPoleCount = 0;

		for (int j = 0; j < 3; ++j)
		{
			if (isPole[j])
				continue;

			const float u = outVertices[i + j].u;

			if (u < minU) minU = u;
			if (u > maxU) maxU = u;
			++nonPoleCount;
		}

		// 예: 0.98, 0.02 → 0.98, 1.02 => 이음새를 가로지른다.
		if (nonPoleCount > 1 && maxU - minU > 0.5f)
		{
			for (int j = 0; j < 3; ++j)
			{
				if (!isPole[j] && outVertices[i + j].u < 0.5f)
				{
					outVertices[i + j].u += 1.0f;
				}
			}
		}
		// UV 값이 1보다 커지는데 괜찮은가?
		// => 텍스처 주소 모드가 Wrap이면 상관없다.
		// samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; 


		// 극점의 u는 보정된 주변 u의 평균으로 설정
		float sumU = 0.0f;

		for (int j = 0; j < 3; ++j)
		{
			if (!isPole[j])
				sumU += outVertices[i + j].u;
		}

		const float poleU = nonPoleCount > 0
			? sumU / static_cast<float>(nonPoleCount)
			: 0.5f;

		for (int j = 0; j < 3; ++j)
		{
			if (isPole[j])
				outVertices[i + j].u = poleU;
		}
	}
}


// 면별 A, B, C, A, C, D → A, B, C, D
inline void CompactCubeVertices(
	const FVertexTextured(&source)[36],
	FVertexTextured(&destination)[24])
{
	constexpr int offsets[4] = { 0, 1, 2, 5 };

	for (int face = 0; face < 6; ++face)
	{
		for (int vertex = 0; vertex < 4; ++vertex)
		{
			destination[face * 4 + vertex] =
				source[face * 6 + offsets[vertex]];
		}
	}
}

// 인덱스 버퍼용: 기본 아틀라스
inline void BuildCubeAtlasVertices(
	FVertexTextured(&outVertices)[24])
{
	FVertexTextured expanded[36];

	// 기존 36정점 버전 호출: UV 방향 처리도 그대로 재사용
	BuildCubeAtlasVertices(expanded);

	CompactCubeVertices(expanded, outVertices);
}

// 인덱스 버퍼용: 사용자 지정 아틀라스
inline void BuildCubeAtlasVertices(
	FVertexTextured(&outVertices)[24],
	int columns,
	int rows,
	const int(&faceCells)[6])
{
	FVertexTextured expanded[36];

	BuildCubeAtlasVertices(expanded, columns, rows, faceCells);

	CompactCubeVertices(expanded, outVertices);
}

template<size_t N>
inline void BuildSphereTextureMeshIndices(const FVertexSimple(&source)[N],
	TArray<FVertexTextured>& outVertices, TArray<UINT>& outIndices)
{
	TArray<FVertexTextured> expanded;
	BuildSphereTextureVertices(source, expanded);

	outVertices.Reset(0);
	outIndices.Reset(0);

	outVertices.Reserve(static_cast<uint32>(N));
	outIndices.Reserve(static_cast<uint32>(N));

	for (size_t i = 0; i < N; ++i)
	{
		const FVertexTextured& vertex = expanded[i];
		int32 foundIndex = -1;

		// 위치와 UV가 모두 같은 정점을 찾는다.
		// 위치가 같아도 UV가 다르면 별개 취급
		for (int32 j = 0; j < outVertices.Num(); ++j)
		{
			const FVertexTextured& existing = outVertices[j];

			if (existing.x == vertex.x &&
				existing.y == vertex.y &&
				existing.z == vertex.z &&
				existing.u == vertex.u &&
				existing.v == vertex.v)
			{
				foundIndex = j;
				break;
			}
		}

		if (foundIndex >= 0)
		{
			outIndices.Add(static_cast<unsigned int>(foundIndex));
		}
		else
		{
			// Add()가 새 정점의 인덱스를 반환한다.
			const unsigned int newIndex = outVertices.Add(vertex);
			outIndices.Add(newIndex);
		}
	}
}
