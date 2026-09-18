#pragma once
#include "Vector.h"
#include "MathUtility.h"
#include "Rotator.h"
#include "Core/enum.h"

struct FMatrix {
    static FMatrix Ortho(float Left, float Right, float Bottom, float Top, float NearZ, float FarZ)
    {
        FMatrix Result{};
        Result.M[0][0] = 2.0f / (Right - Left);
        Result.M[1][1] = 2.0f / (Top - Bottom);
        Result.M[2][2] = 1.0f / (FarZ - NearZ);
        Result.M[3][0] = -(Right + Left) / (Right - Left);
        Result.M[3][1] = -(Top + Bottom) / (Top - Bottom);
        Result.M[3][2] = -NearZ / (FarZ - NearZ);
        Result.M[3][3] = 1.0f;
        return Result;
    }


	float M[4][4];

	static const FMatrix Identity;
	static const FMatrix Zero;

	// 언리얼 좌표계(X 전방 / Y 우측 / Z 상방)를
	// DirectX NDC(X 우측 / Y 위 / Z 화면 안쪽)로 바꾸는 축 교환 행렬.
	static const FMatrix UEToDX;
	

	static FMatrix makeIdentity(); // 단위행렬 만드는 함수

	FMatrix operator* (const FMatrix& Other) const;

	FMatrix operator*(float Scalar) const;

	FMatrix operator+ (const FMatrix& Other) const;

	FMatrix operator- (const FMatrix& Other) const;

	FMatrix operator+(float f) const;

	FMatrix operator-(float f) const;

	bool operator==(const FMatrix& m) const;

	bool operator!=(const FMatrix& m) const;

	// 부동소수 오차를 감안한 비교.
	// 곱셈이나 역행렬로 만들어낸 행렬끼리는 == 대신 이쪽을 써야 한다
	bool Equals(const FMatrix& m, float Tolerance = KINDA_SMALL_NUMBER) const;

	FMatrix Transpose() const;

	static FMatrix Scale(float n);
	static FMatrix Scale(const FVector v);

	static FMatrix RotateX(float degree); // Roll : X축 회전
	static FMatrix RotateY(float degree); // Pitch : Y축 회전
	static FMatrix RotateZ(float degree); // Yaw : Z축 회전
	static FMatrix Rotate(const FRotator r);
	// Rotate matrix from quaternion.
	static FMatrix Rotate(const FQuat q);

	static FMatrix Translation(const FVector v);

	[[nodiscard]] FVector GetUnitAxis(EAxis Axis) const;

	// 위치 변환 (w = 1, 이동 포함).  행벡터 규약 v x M
	[[nodiscard]] FVector TransformPosition(const FVector& V) const;

	// 방향 변환 (w = 0, 이동 제외)
	[[nodiscard]] FVector TransformVector(const FVector& V) const;

	// 아핀 행렬(마지막 열이 0,0,0,1)의 역행렬.
	[[nodiscard]] FMatrix Inverse() const;

	FVector GetTranslation() const;
	FVector GetScale() const;

	// end Struct Matrix
};
