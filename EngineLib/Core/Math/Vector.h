#pragma once
#include "MathUtility.h"

typedef struct FVector
{
	float x, y, z;

	FVector();
	FVector(float n);
	FVector(float _x, float _y, float _z);

	float& operator[](int Index);
	const float& operator[](int Index) const;

	const FVector operator-(const FVector& other) const;
	const FVector operator+(const FVector& other) const;

	void operator+=(const FVector& other);
	void operator-=(const FVector& other);

	// Return the negation of this vector.
	FVector operator-() const;

	//내
	static float dot(const FVector& A, const FVector& B);

	//외적
	static FVector cross(const FVector& A, const FVector& B);

	float Length() const;
	float LengthSquared() const;

	void Normalize();
	FVector GetNormalized() const;

	bool IsNearlyZero(float Tolerance = KINDA_SMALL_NUMBER) const;

	static FVector Up();		// z-axis
	static FVector Right();		// y-axis
	static FVector Forward();	// x-axis
} FVector3;

const FVector operator*(const FVector& v, float f);
const FVector operator*(float f, const FVector& v);

//Vector 4
typedef struct FVector4
{
	float x, y, z, w;
	FVector4(float _x = 0, float _y = 0, float _z = 0, float _w = 0) : x(_x), y(_y), z(_z), w(_w) {}

	const FVector4 operator-(const FVector4& other) const;
	const FVector4 operator+(const FVector4& other) const;

	void operator+=(const FVector4& other);
	void operator-=(const FVector4& other);

	//내적
	inline static float dot(const FVector4& A, const FVector4& B);

	//4차원에는 외적이 없다.

	float Length() const;

} FVector4;

const FVector4 operator*(const FVector4& v, float f);
const FVector4 operator*(float f, const FVector4& v);

// Vector 2
struct FVector2
{
	float x, y;
	FVector2(float _x = 0, float _y = 0);
	const FVector2 operator-(const FVector2& other) const;
	const FVector2 operator+(const FVector2& other) const;

	void operator+=(const FVector2& other);
	void operator-=(const FVector2& Others);

	//내적
	static float dot(const FVector2& A, const FVector2& B);

	void Normalize();
	FVector2 GetNormalized() const;

	float Length() const;
	float LengthSquared() const;
};


