#pragma once

#include <cmath>

#include "Matrix.h"
#include "Core/Math/FBoundingBox.h"

struct FPlane
{
	float A = 0.0f;
	float B = 0.0f;
	float C = 0.0f;
	float D = 0.0f;

	void Normalize()
	{
		const float length = std::sqrt(A * A + B * B + C * C);

		if (length <= 0.000001f)
		{
			return;
		}

		A /= length;
		B /= length;
		C /= length;
		D /= length;
	}

	float Distance(const FVector3& point) const
	{
		return A * point.x + B * point.y + C * point.z + D;
	}
};


struct FFrustum
{
	FPlane Left;
	FPlane Right;
	FPlane Bottom;
	FPlane Top;
	FPlane Near;
	FPlane Far;

	static FFrustum FrustumFromViewProjection(const FMatrix& M)
	{
		FFrustum result;

		// Row Vector 기준
		// ClipPosition = WorldPosition * ViewProjection
		// 그래서 Plane 추출 시 열을 사용한다.

		// Left : x + w >= 0
		result.Left =
		{
			M.M[0][0] + M.M[0][3],
			M.M[1][0] + M.M[1][3],
			M.M[2][0] + M.M[2][3],
			M.M[3][0] + M.M[3][3]
		};

		// Right : w - x >= 0
		result.Right =
		{
			M.M[0][3] - M.M[0][0],
			M.M[1][3] - M.M[1][0],
			M.M[2][3] - M.M[2][0],
			M.M[3][3] - M.M[3][0]
		};

		// Bottom : y + w >= 0
		result.Bottom =
		{
			M.M[0][1] + M.M[0][3],
			M.M[1][1] + M.M[1][3],
			M.M[2][1] + M.M[2][3],
			M.M[3][1] + M.M[3][3]
		};

		// Top : w - y >= 0
		result.Top =
		{
			M.M[0][3] - M.M[0][1],
			M.M[1][3] - M.M[1][1],
			M.M[2][3] - M.M[2][1],
			M.M[3][3] - M.M[3][1]
		};

		// Near : z >= 0
		result.Near =
		{
			M.M[0][2],
			M.M[1][2],
			M.M[2][2],
			M.M[3][2]
		};

		// Far : w - z >= 0
		result.Far =
		{
			M.M[0][3] - M.M[0][2],
			M.M[1][3] - M.M[1][2],
			M.M[2][3] - M.M[2][2],
			M.M[3][3] - M.M[3][2]
		};

		result.Left.Normalize();
		result.Right.Normalize();
		result.Bottom.Normalize();
		result.Top.Normalize();
		result.Near.Normalize();
		result.Far.Normalize();

		return result;
	}

	bool Intersects(const FBoundingBox& bounds) const
	{
		const FPlane planes[6] =
		{
			Left,
			Right,
			Bottom,
			Top,
			Near,
			Far
		};

		for (const FPlane& plane : planes)
		{
			// Plane normal 방향으로 가장 멀리 있는 AABB 정점
			FVector3 positiveVertex;

			positiveVertex.x =
				plane.A >= 0.0f ? bounds.Max.x : bounds.Min.x;

			positiveVertex.y =
				plane.B >= 0.0f ? bounds.Max.y : bounds.Min.y;

			positiveVertex.z =
				plane.C >= 0.0f ? bounds.Max.z : bounds.Min.z;

			// Plane 방향으로 가장 멀리 있는 점조차 바깥이면
			// AABB 전체가 Frustum 밖
			if (plane.Distance(positiveVertex) < 0.0f)
			{
				return false;
			}
		}

		return true;
	}
};
