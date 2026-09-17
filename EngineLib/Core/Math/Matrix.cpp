#include "Matrix.h"

FMatrix FMatrix::makeIdentity()
{
	FMatrix R = {};
	R.M[0][0] = R.M[1][1] = R.M[2][2] = R.M[3][3] = 1.0f;
	return R;
}

FMatrix FMatrix::operator* (const FMatrix& Other) const
{
	FMatrix result = {};

	for (int row = 0; row < 4;++row) {
		for (int col = 0;col < 4;++col) {
			for (int k = 0;k < 4;++k) {
				result.M[row][col] += M[row][k] * Other.M[k][col];
			}
		}
	}
	return result;
}

FMatrix FMatrix::operator*(float Scalar) const
{ 
	FMatrix result;
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			result.M[row][col] = M[row][col] * Scalar;
		}
	}

	return result;
}

FMatrix FMatrix::operator+ (const FMatrix& Other) const
{ 
	FMatrix result = {};

	for (int row = 0; row < 4;++row) {
		for (int col = 0;col < 4;++col) {
			result.M[row][col] = M[row][col] + Other.M[row][col];
		}
	}
	return result;
}

FMatrix FMatrix::operator- (const FMatrix& Other) const
{ 
	FMatrix result = {};

	for (int row = 0; row < 4;++row) {
		for (int col = 0;col < 4;++col) {
			result.M[row][col] = M[row][col] - Other.M[row][col];
		}
	}
	return result;
}

FMatrix FMatrix::operator+(float f) const
{
	FMatrix result = {};
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			result.M[row][col] = M[row][col] + f;
		}
	}
	return result;
}

FMatrix FMatrix::operator-(float f) const
{ 
	FMatrix result={};
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			result.M[row][col] = M[row][col] - f;
		}
	}
	return result;
}

bool FMatrix::operator==(const FMatrix& m) const
{
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			if (M[row][col] != m.M[row][col]) {
				return false;
			}
		}
	}
	return true;
}

bool FMatrix::operator!=(const FMatrix& m) const
{
	return !(*this == m);
}

bool FMatrix::Equals(const FMatrix& m, float Tolerance) const
{
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			if (FMath::Abs(M[row][col] - m.M[row][col]) > Tolerance) {
				return false;
			}
		}
	}
	return true;
}

FMatrix FMatrix::Transpose() const
{ 
	FMatrix result = {};
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			result.M[row][col] = M[col][row];
		}
	}
	return result;
}

FMatrix FMatrix::Scale(float n)
{
	FMatrix result = Identity;
	result.M[0][0] = n;
	result.M[1][1] = n;
	result.M[2][2] = n;

	return result;
}

FMatrix FMatrix::Scale(const FVector v)
{
	FMatrix result = Identity;
	result.M[0][0] = v.x;
	result.M[1][1] = v.y;
	result.M[2][2] = v.z;

	return result;
}

FMatrix FMatrix::RotateX(float degree)
{
	FMatrix result = Identity;
	float s, c;
	FMath::sincos<float>(s, c, degree * PI / 180);

	result.M[1][1] = c;
	result.M[1][2] = -s;
	result.M[2][1] = s;
	result.M[2][2] = c;

	return result;
}

FMatrix FMatrix::RotateY(float degree)
{
	FMatrix result = Identity;
	float s, c;
	FMath::sincos<float>(s, c, degree * PI / 180);

	result.M[0][0] = c;
	result.M[0][2] = s;
	result.M[2][0] = -s;
	result.M[2][2] = c;

	return result;
}

FMatrix FMatrix::RotateZ(float degree)
{
	FMatrix result = Identity;
	float s, c;
	FMath::sincos<float>(s, c, degree * PI / 180);

	result.M[0][0] = c;
	result.M[0][1] = s;
	result.M[1][0] = -s;
	result.M[1][1] = c;

	return result;
}

// NOTE: Row vector convention. v' = v * M
//	|	1 - 2 (y^2 + z^2)	2xy + 2wz			2xz - 2wy			|
//	|	2xy - 2wz			1 - 2 (x^2 + z^2)	2yz + 2wx			|
//	|	2xz + 2wy			2yz - 2wx			1 - 2 (x^2 + y^2)	|
FMatrix FMatrix::Rotate(const FRotator r)
{
	//Pitch, Yaw, Roll의 각각 cossin 구하기
	FMatrix Matrix = FMatrix::Identity;
	float cosP, cosY, cosR;
	float sinP, sinY, sinR;

	//도 -> 라디안 변환 후 sincos 호출
	FMath::sincos<float>(sinP, cosP, r.Pitch * PI / 180);
	FMath::sincos<float>(sinY, cosY, r.Yaw * PI / 180);
	FMath::sincos<float>(sinR, cosR, r.Roll * PI / 180);

	Matrix.M[0][0] = cosP * cosY;
	Matrix.M[0][1] = cosP * sinY;
	Matrix.M[0][2] = sinP;
	Matrix.M[1][0] = sinR * sinP * cosY - cosR * sinY;
	Matrix.M[1][1] = sinR * sinP * sinY + cosR * cosY;
	Matrix.M[1][2] = -sinR * cosP;
	Matrix.M[2][0] = -(cosR * sinP * cosY + sinR * sinY);
	Matrix.M[2][1] = sinR * cosY - cosR * sinP * sinY;
	Matrix.M[2][2] = cosR * cosP;

	return Matrix;
}

FMatrix FMatrix::Rotate(const FQuat q)
{
	FMatrix result = Identity;
	const float x2 = q.x + q.x;
	const float y2 = q.y + q.y;
	const float z2 = q.z + q.z;
	const float xx2 = q.x * x2;
	const float yy2 = q.y * y2;
	const float zz2 = q.z * z2;
	result.M[0][0] = 1.0f - (yy2 + zz2);
	result.M[1][1] = 1.0f - (xx2 + zz2);
	result.M[2][2] = 1.0f - (xx2 + yy2);
	const float yz2 = q.y * z2;
	const float wx2 = q.w * x2;
	result.M[1][2] = yz2 + wx2;
	result.M[2][1] = yz2 - wx2;
	const float xy2 = q.x * y2;
	const float wz2 = q.w * z2;
	result.M[0][1] = xy2 + wz2;
	result.M[1][0] = xy2 - wz2;
	const float xz2 = q.x * z2;
	const float wy2 = q.w * y2;
	result.M[0][2] = xz2 - wy2;
	result.M[2][0] = xz2 + wy2;

	return result;
}

FMatrix FMatrix::Translation(const FVector v)
{
	FMatrix result = Identity;
	result.M[3][0] = v.x;
	result.M[3][1] = v.y;
	result.M[3][2] = v.z;

	return result;
}

FVector FMatrix::GetUnitAxis(EAxis Axis) const
{
	const int i = static_cast<int>(Axis);
	return FVector(M[i][0], M[i][1], M[i][2]);
}

FVector FMatrix::TransformPosition(const FVector& V) const
{
	return FVector(
		V.x * M[0][0] + V.y * M[1][0] + V.z * M[2][0] + M[3][0],
		V.x * M[0][1] + V.y * M[1][1] + V.z * M[2][1] + M[3][1],
		V.x * M[0][2] + V.y * M[1][2] + V.z * M[2][2] + M[3][2]);
}

FVector FMatrix::TransformVector(const FVector& V) const
{
	return FVector(
		V.x * M[0][0] + V.y * M[1][0] + V.z * M[2][0],
		V.x * M[0][1] + V.y * M[1][1] + V.z * M[2][1],
		V.x * M[0][2] + V.y * M[1][2] + V.z * M[2][2]);
}

// MakeMatrix() 결과가 항상 이 형태라 일반 4x4 역행렬이 필요 없다.
//   M = | A 0 |        M^-1 = | A^-1     0 |
//       | t 1 |               | -t*A^-1  1 |
// Transpose() 와 달리 비균등 스케일에도 동작한다.
FMatrix FMatrix::Inverse() const
{
	const float C00 =  (M[1][1] * M[2][2] - M[1][2] * M[2][1]);
	const float C01 = -(M[1][0] * M[2][2] - M[1][2] * M[2][0]);
	const float C02 =  (M[1][0] * M[2][1] - M[1][1] * M[2][0]);

	const float Det = M[0][0] * C00 + M[0][1] * C01 + M[0][2] * C02;
	if (FMath::Abs(Det) < SMALL_NUMBER)
	{
		return FMatrix::Zero;   // 스케일 0 등 역행렬이 없는 경우
	}

	const float C10 = -(M[0][1] * M[2][2] - M[0][2] * M[2][1]);
	const float C11 =  (M[0][0] * M[2][2] - M[0][2] * M[2][0]);
	const float C12 = -(M[0][0] * M[2][1] - M[0][1] * M[2][0]);
	const float C20 =  (M[0][1] * M[1][2] - M[0][2] * M[1][1]);
	const float C21 = -(M[0][0] * M[1][2] - M[0][2] * M[1][0]);
	const float C22 =  (M[0][0] * M[1][1] - M[0][1] * M[1][0]);

	const float Inv = 1.0f / Det;

	FMatrix R = FMatrix::Identity;

	// 수반행렬 = 여인수 행렬의 전치
	R.M[0][0] = C00 * Inv;  R.M[0][1] = C10 * Inv;  R.M[0][2] = C20 * Inv;
	R.M[1][0] = C01 * Inv;  R.M[1][1] = C11 * Inv;  R.M[1][2] = C21 * Inv;
	R.M[2][0] = C02 * Inv;  R.M[2][1] = C12 * Inv;  R.M[2][2] = C22 * Inv;

	// 이동 성분 : -t * A^-1
	R.M[3][0] = -(M[3][0] * R.M[0][0] + M[3][1] * R.M[1][0] + M[3][2] * R.M[2][0]);
	R.M[3][1] = -(M[3][0] * R.M[0][1] + M[3][1] * R.M[1][1] + M[3][2] * R.M[2][1]);
	R.M[3][2] = -(M[3][0] * R.M[0][2] + M[3][1] * R.M[1][2] + M[3][2] * R.M[2][2]);

	return R;
}

FVector FMatrix::GetTranslation() const
{
	return FVector(M[3][0], M[3][1], M[3][2]);
}

FVector FMatrix::GetScale() const
{
	float scaleX = std::sqrt(M[0][0] * M[0][0] + M[1][0] * M[1][0] + M[2][0] * M[2][0]);
	float scaleY = std::sqrt(M[0][1] * M[0][1] + M[1][1] * M[1][1] + M[2][1] * M[2][1]);
	float scaleZ = std::sqrt(M[0][2] * M[0][2] + M[1][2] * M[1][2] + M[2][2] * M[2][2]);
	return FVector(scaleX, scaleY, scaleZ);
}

const FMatrix FMatrix::Identity = { {
	{ 1, 0, 0, 0 },
	{ 0, 1, 0, 0 },
	{ 0, 0, 1, 0 },
	{ 0, 0, 0, 1 }
} };

const FMatrix FMatrix::Zero = { {
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 }
} };

const FMatrix FMatrix::UEToDX = { {
	{ 0, 0, 1, 0 },
	{ 1, 0, 0, 0 },
	{ 0, 1, 0, 0 },
	{ 0, 0, 0, 1 }
} };
