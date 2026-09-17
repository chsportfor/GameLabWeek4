#include "Vector.h"

FVector::FVector() : x(0), y(0), z(0) {}
FVector::FVector(float n) : x(n), y(n), z(n) {}
FVector::FVector(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

float& FVector::operator[](int Index)
{
	switch (Index)
	{
	case 0:
		return x;
	case 1:
		return y;
	default:
		return z;
	}
}

const float& FVector::operator[](int Index) const
{
	switch (Index)
	{
	case 0:
		return x;
	case 1:
		return y;
	default:
		return z;
	}
}

const FVector FVector::operator-(const FVector& other) const
{
	return FVector(x - other.x, y - other.y, z - other.z);
}

const FVector FVector::operator+(const FVector& other) const
{
	return FVector(x + other.x, y + other.y, z + other.z);
}

void FVector::operator+=(const FVector& other)
{
	x += other.x;
	y += other.y;
	z += other.z;
}

void FVector::operator-=(const FVector& other)
{
	x -= other.x;
	y -= other.y;
	z -= other.z;
}

FVector operator+(const FVector& A, const FVector& B)
{
	return FVector(A.x + B.x, A.y + B.y, A.z + B.z);
}

FVector FVector::operator-() const
{
	return FVector(-x, -y, -z);
}

float FVector::dot(const FVector& A, const FVector& B)
{
	return A.x * B.x + A.y * B.y + A.z * B.z;
}

FVector FVector::cross(const FVector& A, const FVector& B)
{
	return FVector(A.y * B.z - A.z * B.y, A.z * B.x - A.x * B.z, A.x * B.y - A.y * B.x);
}

float FVector::Length() const
{
	return FMath::Sqrt(x * x + y * y + z * z);
}

float FVector::LengthSquared() const
{
	return x * x + y * y + z * z;
}

void FVector::Normalize()
{
	float len = Length();
	if (len > KINDA_SMALL_NUMBER)
	{
		x /= len;
		y /= len;
		z /= len;
	}
	else
	{
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
	}
}

FVector FVector::GetNormalized() const
{
	float len = Length();
	if (len > KINDA_SMALL_NUMBER)
	{
		return FVector(x / len, y / len, z / len);
	}
	else
	{
		return FVector(0.0f, 0.0f, 0.0f);
	}
}

bool FVector::IsNearlyZero(float Tolerance) const
{
	return LengthSquared() < Tolerance;
}

FVector FVector::Up()
{
	return FVector(0, 0, 1);
}

FVector FVector::Right()
{
	return FVector(0, 1, 0);
}

FVector FVector::Forward()
{
	return FVector(1, 0, 0);
}

const FVector operator*(const FVector& v, float f)
{
	return FVector(v.x * f, v.y * f, v.z * f);
}

const FVector operator*(float f, const FVector& v)
{
	return FVector(v.x * f, v.y * f, v.z * f);
}

const FVector4 FVector4::operator-(const FVector4& other) const
{
	return FVector4(x - other.x, y - other.y, z - other.z, w - other.w);
}

const FVector4 FVector4::operator+(const FVector4& other) const
{
	return FVector4(x + other.x, y + other.y, z + other.z, w + other.w);
}

void FVector4::operator+=(const FVector4& other)
{
	x += other.x;
	y += other.y;
	z += other.z;
	w += other.w;
}

void FVector4::operator-=(const FVector4& other)
{
	x -= other.x;
	y -= other.y;
	z -= other.z;
	w -= other.w;
}

float FVector4::dot(const FVector4& A, const FVector4& B)
{
	return A.x * B.x + A.y * B.y + A.z * B.z + A.w * B.w;
}

float FVector4::Length() const
{
	return FMath::Sqrt(x * x + y * y + z * z + w * w);
}

const FVector4 operator*(const FVector4& v, float f)
{
	return FVector4(v.x * f, v.y * f, v.z * f, v.w * f);
}

const FVector4 operator*(float f, const FVector4& v)
{
	return FVector4(v.x * f, v.y * f, v.z * f, v.w * f);
}

FVector2::FVector2(float _x, float _y) : x(_x), y(_y) {}

const FVector2 FVector2::operator-(const FVector2& other) const
{
	return FVector2(x - other.x, y - other.y);
}

const FVector2 FVector2::operator+(const FVector2& other) const
{
	return FVector2(x + other.x, y + other.y);
}

void FVector2::operator+=(const FVector2& other)
{
	x += other.x;
	y += other.y;
}

void FVector2::operator-=(const FVector2& other)
{
	x -= other.x;
	y -= other.y;
}

float FVector2::dot(const FVector2& A, const FVector2& B)
{
	return A.x * B.x + A.y * B.y;
}

void FVector2::Normalize()
{
	float len = Length();
	if (len > KINDA_SMALL_NUMBER)
	{
		x /= len;
		y /= len;
	}
	else
	{
		x = 0.0f;
		y = 0.0f;
	}
}

FVector2 FVector2::GetNormalized() const
{
	float len = Length();
	if (len > KINDA_SMALL_NUMBER)
	{
		return FVector2(x / len, y / len);
	}
	else
	{
		return FVector2(0.0f, 0.0f);
	}
}

float FVector2::Length() const
{
	return FMath::Sqrt(x * x + y * y);
}

float FVector2::LengthSquared() const
{
	return x * x + y * y;
}
