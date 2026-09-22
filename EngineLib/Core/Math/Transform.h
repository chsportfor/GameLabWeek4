#pragma once
#include "Vector.h"
#include "Rotator.h"
#include "Quat.h"
#include "Matrix.h"
#include <cassert>

// 에디터에서 허용하는 스케일 하한. 역변환은 각 축의 안전한 역수를 사용한다.
constexpr float MIN_SCALE = 0.001f;

struct FTransform
{
	FTransform(){ }
	FTransform(FVector _Location, FRotator _Rotation, FVector _Scale) : Location(_Location), Rotation(_Rotation.Quaternion()), Scale(_Scale)
	{
	}
	FTransform(FVector _Location, FQuat _Rotation, FVector _Scale) : Location(_Location), Rotation(_Rotation), Scale(_Scale)
	{
	}
	FVector Location = FVector(0);
	FQuat Rotation = FQuat(0, 0, 0, 1);
	FVector Scale = FVector(1);

	FVector GetLocation() const { return Location; }
	void SetLocation(const FVector& InLocation) { Location = InLocation; }
	FQuat GetRotation() const { return Rotation; }
	FRotator GetRotator() const { return Rotation.Rotator(); }
	void SetRotation(const FQuat& InRotation) { Rotation = InRotation; }
	void SetRotation(const FRotator& InRotator) { Rotation = InRotator.Quaternion(); }
	FVector GetScale() const { return Scale; }
	void SetScale(const FVector& InScale) { Scale = InScale; }

	FVector InverseTransformPosition(const FVector& Position) const
	{
		const FVector translated = Position - Location;
		const FVector unrotated = FMatrix::Rotate(Rotation).Transpose().TransformVector(translated);
		const FVector inverseScale(
			FMath::Abs(Scale.x) <= SMALL_NUMBER ? 0.0f : 1.0f / Scale.x,
			FMath::Abs(Scale.y) <= SMALL_NUMBER ? 0.0f : 1.0f / Scale.y,
			FMath::Abs(Scale.z) <= SMALL_NUMBER ? 0.0f : 1.0f / Scale.z);
		return FVector(
			unrotated.x * inverseScale.x,
			unrotated.y * inverseScale.y,
			unrotated.z * inverseScale.z);
	}

	FMatrix MakeMatrix() const
	{
		FMatrix result = FMatrix::Rotate(Rotation);

		for (int Col = 0; Col < 3; ++Col)
		{
			result.M[0][Col] *= Scale.x;
			result.M[1][Col] *= Scale.y;
			result.M[2][Col] *= Scale.z;
		}

		// 이동 성분은 마지막 행에 저장한다.
		result.M[3][0] = Location.x;
		result.M[3][1] = Location.y;
		result.M[3][2] = Location.z;

		return result;
	}

	FMatrix InverseMatrix() const
	{
		assert(Scale.x == 0.f || Scale.y == 0.f || Scale.z == 0.f);

		return {
			FMatrix::Translation(FVector(-Location.x, -Location.y, -Location.z))
			* FMatrix::Rotate(Rotation).Transpose()
			* FMatrix::Scale(FVector(1.0f / Scale.x, 1.0f / Scale.y, 1.0f / Scale.z))
		};
	}


};
