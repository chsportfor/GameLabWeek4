#pragma once
#include <cmath>
#include "Vector.h"
#include "MathUtility.h"

#include "Quat.h"

struct FRotator
{
	float Pitch, Yaw, Roll;

	FRotator() = default;

	FRotator(float InPitch, float InYaw, float InRoll)
		: Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}

	//static const FRotator ZeroRotator = { 0, 0, 0 };

	static FRotator FromDirection(const FVector& Direction)
	{
		float Yaw = std::atan2(Direction.y, Direction.x);
		float Pitch = std::atan2(Direction.z, std::sqrt(Direction.x * Direction.x
													  + Direction.y * Direction.y));

		return FRotator(Pitch * 180 / PI, Yaw * 180 / PI, 0.0f);
	}

	// From 위치에서 To 위치를 바라보는 회전값.
	static FRotator LookAt(const FVector& From, const FVector& To)
	{
		return FromDirection(To - From);
	}

	[[nodiscard]] FVector Vector() const
	{
		const float PitchNoWinding = FMath::Fmod(Pitch, 360.0f);
		const float YawNoWinding = FMath::Fmod(Yaw, 360.0f);

		float CP, SP, CY, SY;
		FMath::sincos<float>(SP, CP, FMath::DegreesToRadians(PitchNoWinding));
		FMath::sincos<float>(SY, CY, FMath::DegreesToRadians(YawNoWinding));
		FVector V = FVector(CP * CY, CP * SY, SP);

		return V;
	}

	// Convert this rotator to a quaternion.
	// v' = q * v * q^-1
	// q = qz * qy * qx
	// qx = (sin(roll/2), 0, 0, cos(roll/2))
	// qy = (0, sin(pitch/2), 0, cos(pitch/2))
	// qz = (0, 0, sin(yaw/2), cos(yaw/2))
	FQuat Quaternion() const
	{
		// Convert sign of pitch and roll to match Unreal Engine's coordinate system
		// Roll and pitch are turning clockwise, while yaw is turning counter-clockwise.
		const float halfRoll = FMath::DegreesToRadians(-Roll) * 0.5f;
		const float halfPitch = FMath::DegreesToRadians(-Pitch) * 0.5f;
		const float halfYaw = FMath::DegreesToRadians(Yaw) * 0.5f;

		FQuat qx = { std::sin(halfRoll), 0, 0, std::cos(halfRoll) };
		FQuat qy = { 0, std::sin(halfPitch), 0, std::cos(halfPitch) };
		FQuat qz = { 0, 0, std::sin(halfYaw), std::cos(halfYaw) };

		return qz * qy * qx;
	}
};
