#pragma once
#include "Core/Math/Transform.h"
#include <cmath>
#include "Core/Math/Vector.h"

class FCamera
{
public:
    static constexpr float NearPlane = .1f;
    static constexpr float FarPlane = 100.f;
	FCamera() : Location({ -2.0f, 1.0f, 1.0f })
	{
		LookAt({ 0, 0, 0 });
	}

	FCamera(FVector _Location, FRotator _Rotation) : Location(_Location), Rotation(_Rotation) {}
	FVector Location;
	FRotator Rotation;

	const FRotator GetRotation() const { return Rotation; }

	FMatrix GetViewMatrix() const
	{
		// 카메라에는 스케일이 없다. 위치를 되돌리고, 회전을 되돌리고, 축을 교환한다.
		return FMatrix::Translation(FVector(-Location.x,
			-Location.y,
			-Location.z))
			* FMatrix::Rotate(Rotation).Transpose()
			* FMatrix::UEToDX;
	}

	// 특정 지점을 바라보도록 회전을 맞춘다.
	void LookAt(const FVector& Target)
	{
		Rotation = FRotator::LookAt(Location, Target);
	}

	// Unified matrix for projection
	FMatrix GetUnifiedProjectionMatrix(float aspect, float fovDegree, float d, float n, float f, float t) const
	{
		const float sy = 1.0f / tanf((fovDegree / 2) * PI / 180); //yScale
		const float sx = sy / aspect;

		const float A = ((1.0f - t) + t * f / d) / (f - n);

		FMatrix result = FMatrix::Zero;
		result.M[0][0] = sx / d;
		result.M[1][1] = sy / d;
		result.M[2][2] = A;
		result.M[2][3] = t / d;
		result.M[3][2] = -n * A;
		result.M[3][3] = 1.0f - t;

		return result;
	}

	// Inverse matrix for unified projection matrix
	FMatrix GetInverseUnifiedProjectionMatrix(float aspect, float fovDegree, float d, float n, float f, float t) const
	{
		const FMatrix P = GetUnifiedProjectionMatrix(aspect, fovDegree, d, n, f, t);

		const float A = P.M[2][2];
		const float B = P.M[2][3];
		const float C = P.M[3][2];
		const float D = P.M[3][3];

		const float degt = A * D - B * C;

		FMatrix result = FMatrix::Zero;
		result.M[0][0] = 1.0f / P.M[0][0];
		result.M[1][1] = 1.0f / P.M[1][1];
		result.M[2][2] = D / degt;
		result.M[2][3] = -B / degt;
		result.M[3][2] = -C / degt;
		result.M[3][3] = A / degt;

		return result;
	}

	void Rotate(long Dx, long Dy)
	{
		Rotation.Yaw += FMath::Fmod(Dx * Sensitivity, 360.f);
		Rotation.Pitch -= FMath::Fmod(Dy * Sensitivity, 360.f);
	}


	FVector GetForwardVector() const { return FMatrix::Rotate(Rotation).GetUnitAxis(EAxis::X); }
	FVector GetRightVector()   const { return FMatrix::Rotate(Rotation).GetUnitAxis(EAxis::Y); }
	FVector GetUpVector()      const { return FMatrix::Rotate(Rotation).GetUnitAxis(EAxis::Z); }

	//속력
	float Speed = 5.f;

	//속도
	FVector Velocity = FVector(0);

	//카메라 이동 민감도
	float Sensitivity = 0.1f;

	void SetCameraSensitivity(float InSensitivity)
	{
		Sensitivity = InSensitivity;
	}

	//카메라 시야각
	float mFovDegree = 60.f;

	// 직교 투영에서 카메라와 화면 사이의 거리
	float mOrthoDistance = 5.0f;


	//감속 계수(1/초). 클수록 빨리 멈춘다
	float Damping = 6.f;
};
