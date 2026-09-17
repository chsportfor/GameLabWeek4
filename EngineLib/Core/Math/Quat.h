#pragma once

struct FRotator;

struct FQuat
{
	float x, y, z, w;

	FQuat(float _x = 0, float _y = 0, float _z = 0, float _w = 1) : x(_x), y(_y), z(_z), w(_w) {}

	FQuat operator*(const FQuat& Others) const
	{
		return FQuat(
			w * Others.x + x * Others.w + y * Others.z - z * Others.y,
			w * Others.y - x * Others.z + y * Others.w + z * Others.x,
			w * Others.z + x * Others.y - y * Others.x + z * Others.w,
			w * Others.w - x * Others.x - y * Others.y - z * Others.z
		);
	}

	FRotator Rotator() const;

	FQuat static Identity()
	{
		return FQuat(0, 0, 0, 1);
	}
};
