#pragma once

#include "Core/Container/TArray.h"

#include "VertexType.h"

class FFontResource;

// 폰트 방식
enum class EFontRenderMode
{
	Bitmap,
	MSDF
};

struct FTextMesh
{
	TArray<FVertexSimple> Vertices;
	TArray<uint32> Indices;

	uint32 TextNum = 0; // Number of characters in the text

	EFontRenderMode FontRenderMode = EFontRenderMode::Bitmap;

	FTextMesh() = default;
	FTextMesh(const FString& text, const FFontResource& fontResource);
	void SetText(const FString& text, const FFontResource& fontResource);

	// scale: 1em을 월드 공간에서 얼마로 표시할지
	void SetUnicodeText(
		const FString& text,
		const FFontResource& fontResource,
		float scale = 0.2f);
private:
	// Helper function to generate the mesh for the given text
	void generateMesh(const FString& text, const FFontResource& fontResource);
	void generateUnicodeMesh(
		const FString& text,
		const FFontResource& fontResource,
		float scale);
	// string_view: 읽기 전용 뷰만 제공, 복사 없음
	bool DecodeNextUTF8(std::string_view text,
		size_t& offset,
		uint32& outCodePoint);
};
