#pragma once

#include "Core/Container/TMap.h"

struct FCharacterInfo
{
	// 텍스처 내 UV 영역
	float U;
	float V;
	float UVWidth;
	float UVHeight;

	// 글자 사각형 크기
	// ASCII: 기존 월드 단위
	// Unicode: JSON의 em 단위 메시 생성 시 scale 적용
	float Width;
	float Height;

	// ASCII 전용: 기존 Width * Advance 계산 유지
	float Advance;

	// Unicode 전용: 기준점에서 사각형까지의 오프셋
	float BearingX = 0.0f;
	float BearingY = 0.0f;

	// Unicode 전용: 다음 글자까지의 이동 거리(em)
	float AdvanceX = 0.0f;

	// 공백처럼 이동 거리만 있고 사각형이 없는 글리프 구분
	// true 기본값으로 기존 ASCII 생성 코드의 동작 유지
	bool HasGeometry = true;
};

class FFontResource
{
public:
	const FCharacterInfo* FindCharacter(char character) const
	{
		return mCharacterMap.Find(character);
	}

	const FCharacterInfo* FindUnicodeCharacter(uint32 codePoint) const
	{
		return mUnicodeCharacterMap.Find(codePoint);
	}

	// 
	// JSON에 저장된 정보 불러오기
	bool LoadUnicodeAtlas(const FString& jsonPath);
	bool LoadUnicodeAtlasFromString(const FString& jsonText);
	uint32 GetAtlasWidth() const { return mAtlasWidth; }
	uint32 GetAtlasHeight() const { return mAtlasHeight; }

	//FFontResource() = default;

	// Build FFontResource from ASCII table. Each character is mapped to a grid cell in the texture atlas.
	FFontResource(int colums = 16, int rows = 16, float charWidth = 0.1f, float charHeight = 0.2f, float charAdvance = 0.7f)
	{
		for (int i = 0; i < colums * rows; ++i)
		{
			char character = static_cast<char>(i);
			FCharacterInfo info{};
			info.U = (i % colums) * (1.0f / colums);
			info.V = (i / colums) * (1.0f / rows);
			info.UVWidth = 1.0f / colums;
			info.UVHeight = 1.0f / rows;
			info.Width = charWidth;
			info.Height = charHeight;
			info.Advance = charAdvance;
			mCharacterMap.Add(character, info);
		}
	}

	float GetDistanceRange() const
	{
		return mDistanceRange;
	}

private:
	TMap<char, FCharacterInfo> mCharacterMap;
	TMap<uint32, FCharacterInfo> mUnicodeCharacterMap;
	float mDistanceRange = 4.0f;
	uint32 mAtlasWidth = 0;
	uint32 mAtlasHeight = 0;
};
