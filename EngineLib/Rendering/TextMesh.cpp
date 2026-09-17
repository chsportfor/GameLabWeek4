#include "TextMesh.h"

#include "Editor/Console.h"
#include "FontResource.h"

FTextMesh::FTextMesh(const FString& text, const FFontResource& fontResource)
{
	generateMesh(text, fontResource);
}

void FTextMesh::SetText(const FString& text, const FFontResource& fontResource)
{
	generateMesh(text, fontResource);
}

void FTextMesh::generateMesh(const FString& text, const FFontResource& fontResource)
{
	FontRenderMode = EFontRenderMode::Bitmap;

	TextNum = text.Len();

	// Variables for center alignment
	float textTotalWidth = 0.0f;
	float textMaxHeight = 0.0f;

	Vertices.Reset(TextNum * 4); // 4 vertices per character
	Indices.Reset(TextNum * 6);  // 6 indices per character (2 triangles)

	uint32 i = 0;
	for (char ch : text)
	{
		const FCharacterInfo* charInfo = fontResource.FindCharacter(ch);

		if (charInfo == nullptr)
		{
			UE_LOG(Warning, Render, "Character '%c' not found in font resource.", ch);
			continue;
		}

		// UV coordinates
		const float u0 = charInfo->U;
		const float u1 = charInfo->U + charInfo->UVWidth;
		const float v0 = charInfo->V;
		const float v1 = charInfo->V + charInfo->UVHeight;

		// Local position of the character quad
		const float advance = charInfo->Width * charInfo->Advance;
		const float left = static_cast<float>(i) * advance;
		const float right = left + charInfo->Width;
		const float top = 0.0f;
		const float bottom = top - charInfo->Height;

		// Update total width and max height
		textTotalWidth += advance;
		textMaxHeight = FMath::Max(textMaxHeight, charInfo->Height);

		// Add vertices for the character quad
		Vertices.Add({ 0.0f, left, top, u0, v0 });
		Vertices.Add({ 0.0f, right, top, u1, v0 });
		Vertices.Add({ 0.0f, right, bottom, u1, v1 });
		Vertices.Add({ 0.0f, left, bottom, u0, v1 });

		// Add indices for the character quad
		uint32 baseIndex = i * 4;
		Indices.Add(baseIndex + 0); // Triangle 1
		Indices.Add(baseIndex + 1);
		Indices.Add(baseIndex + 2);
		Indices.Add(baseIndex + 2); // Triangle 2
		Indices.Add(baseIndex + 3);
		Indices.Add(baseIndex + 0);

		++i;
	}

	// Center the text mesh around the origin
	float offsetX = -textTotalWidth * 0.5f;
	float offsetY = textMaxHeight * 0.5f;

	for (FVertexTextured& vertex : Vertices)
	{
		vertex.y += offsetX;
		vertex.z += offsetY;
	}
}

void FTextMesh::SetUnicodeText(
	const FString& text,
	const FFontResource& fontResource,
	float scale)
{
	generateUnicodeMesh(text, fontResource, scale);
}

// UTF-8 문자열을 읽고, 각 글자를 그릴 사각형의 정점과 인덱스를 만드는 함수
void FTextMesh::generateUnicodeMesh(
	const FString& text,
	const FFontResource& fontResource,
	float scale)
{
	// RenderMode 설정
	FontRenderMode = EFontRenderMode::MSDF;

	Vertices.Reset(TextNum * 4); // 4 vertices per character
	Indices.Reset(TextNum * 6);  // 6 indices per character (2 triangles)

	TextNum = 0; // 실제로 생성한 글자 사각형 수

	if (scale <= 0.0f)
	{
		return;
	}

	// FString 내부 문자열을 복사 없이 참조
	const std::string_view utf8 = static_cast<std::string_view>(text);

	//다음 글자를 배치할 가로 기준 위치
	float penX = 0.0f;
	// 글자가 놓이는 기준선의 높이
	const float baselineZ = 0.0f;

	// 생성한 사각형 중 가장 낮은 위치
	float minZ = 0.0f;
	// 생성한 사각형 중 가장 높은 위치
	float maxZ = 0.0f;
	// 사각형을 하나라도 만들었는지
	bool hasVisibleGlyph = false;

	// 아틀라스에 없는 문자를 만났을 때 사용할 '?'의 정보
	const FCharacterInfo* fallback =
		fontResource.FindUnicodeCharacter(
			static_cast<uint32>('?'));

	// offset: 문자열에서 다음에 읽을 바이트 위치
	// codePoint: 해석한 문자의 유니코드 값
	size_t offset = 0;
	uint32 codePoint = 0;

	// 유니코드 디코딩
	while (DecodeNextUTF8(utf8, offset, codePoint))
	{
		// 이번 예시는 한 줄 표시용.
		// CRLF는 하나의 공백으로 처리.
		// Windows 줄바꿈인 \r\n은 두 문자로 구성되기 때문
		if (codePoint == '\r')
		{
			if (offset < utf8.size() && utf8[offset] == '\n')
			{
				++offset;
			}

			codePoint = ' ';
		}
		// 단독 줄바꿈과 탭도 공백 하나로 변경
		else if (codePoint == '\n' || codePoint == '\t')
		{
			codePoint = ' ';
		}

		// 유니코드 값을 키로 사용해서 글자 정보 찾기
		const FCharacterInfo* info = fontResource.FindUnicodeCharacter(codePoint);

		// 정보가 없으면 '?'로 대체문자 사용
		if (info == nullptr)
		{
			info = fallback;
		}

		// 대체 문자('?')도 없으면 해당 문자를 건너뜀
		if (info == nullptr)
		{
			continue;
		}

		// 그릴 모양이 있는지 체크
		// Why: 공백처럼 이동 거리는 있지만 그릴 사각형은 없는 문자를 체크
		if (info->HasGeometry)
		{
			// 글자 사각형 위치
			// 펜 위치에서 실제 사각형 왼쪽까지의 가로 보정값
			const float left =
				penX + info->BearingX * scale;

			const float right =
				left + info->Width * scale;

			// BearingY는 기준선에서 사각형 위쪽까지의 높이 보정값
			const float top =
				baselineZ + info->BearingY * scale;

			// Z축이 위로 증가하므로, 아래쪽은 top에서 높이를 뺀다.
			const float bottom =
				top - info->Height * scale;

			// 텍스처 좌표
			const float u0 = info->U;
			const float u1 = info->U + info->UVWidth;

			const float v0 = info->V;
			const float v1 = info->V + info->UVHeight;

			// 문자열 위치가 아니라 실제 정점 개수 기준
			const uint32 baseIndex = Vertices.Num();

			// 기존 TextMesh와 같은 축 배치와 정점 순서
			// X = 0, Y = 가로, Z = 세로
			Vertices.Add({ 0.0f, left,  top,    u0, v0 });
			Vertices.Add({ 0.0f, right, top,    u1, v0 });
			Vertices.Add({ 0.0f, right, bottom, u1, v1 });
			Vertices.Add({ 0.0f, left,  bottom, u0, v1 });

			Indices.Add(baseIndex + 0);
			Indices.Add(baseIndex + 1);
			Indices.Add(baseIndex + 2);

			Indices.Add(baseIndex + 2);
			Indices.Add(baseIndex + 3);
			Indices.Add(baseIndex + 0);

			// 첫 사각형이면 그 범위를 그대로 초기값
			// 실제 생성한 사각형의 수직 범위
			if (!hasVisibleGlyph)
			{
				// 아직 범위가 없으므로 첫 글자로 범위를 만든다.
				minZ = bottom;
				maxZ = top;
				hasVisibleGlyph = true;
			}
			else
			{
				// 이미 범위가 있으므로 새 글자를 포함하도록 확장한다.
				minZ = FMath::Min(minZ, bottom);
				maxZ = FMath::Max(maxZ, top);

				// ? x 방향은?
			}
		}

		// 다음 글자 위치로 이동
		// 공백도 이동 거리는 반영
		penX += info->AdvanceX * scale;
	}

	// UTF-8 바이트 수가 아니라 실제 생성한 사각형 수
	TextNum = Vertices.Num() / 4;

	// 빈 문자열 또는 공백만 있는 문자열
	if (!hasVisibleGlyph)
	{
		return;
	}

	// 가로: 공백을 포함한 전체 이동 거리 기준 중앙 정렬
	const float offsetY = -penX * 0.5f;

	// 세로: 실제 사각형 범위 기준 중앙 정렬
	const float offsetZ = -(minZ + maxZ) * 0.5f;

	// 모든 정점에 정렬 적용
	for (FVertexTextured& vertex : Vertices)
	{
		vertex.y += offsetY;
		vertex.z += offsetZ;
	}
}

// 예: "A가" => 41 EA B0 80
// offset: 다음에 읽을 바이트 위치
// 유니코드 디코딩
bool FTextMesh::DecodeNextUTF8(
	std::string_view text,
	size_t& offset,
	uint32& outCodePoint)
{
	// 더 읽을 바이트가 없으면 종료
	if (offset >= text.size())
	{
		return false;
	}

	// 0xFFFD는 유니코드에서 알 수 없거나
	// 잘못된 데이터가 입력되었을 때 깨진 글자 대신 보여주는 대체 문자
	constexpr uint32 ReplacementCharacter = 0xFFFD;

	// char는 환경에 따라 음수가 될 수 있으므로,
	// 비트 처리를 위해 uint8로 바꾸어 0~255
	const uint8 first =
		static_cast<uint8>(text[offset]);

	// ASCII: 0xxxxxxx
	// ASCII이면 바로 반환
	if (first <= 0x7F)
	{
		outCodePoint = first;
		++offset;
		return true;
	}

	size_t byteCount = 0; // 이 문자가 몇 바이트인지
	uint32 codePoint = 0; // 바이트에서 뽑은 비트들을 합칠 공간
	uint32 minimumCodePoint = 0; // 해당 바이트 수로 표현할 수 있는 최소 유효 값

	// 2바이트로 표현되는 범위
	if (first >= 0xC2 && first <= 0xDF)
	{
		// 110xxxxx 10xxxxxx
		byteCount = 2;
		codePoint = first & 0x1F; // 문자 데이터 5비트만 남긴다
		minimumCodePoint = 0x80;
	}
	// 3바이트 문자(한글 완성형 문자가 여기에 해당)
	else if (first >= 0xE0 && first <= 0xEF)
	{
		// 1110xxxx 10xxxxxx 10xxxxxx
		byteCount = 3;
		codePoint = first & 0x0F; // 바이트의 데이터 4비트 남긴다
		minimumCodePoint = 0x800;
	}
	// 4바이트 문자(이모지)
	else if (first >= 0xF0 && first <= 0xF4)
	{
		// 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
		byteCount = 4;
		codePoint = first & 0x07; // 바이트의 데이터 3비트 남긴다
		minimumCodePoint = 0x10000;
	}
	// 그외에는 잘못된 문자
	else
	{
		outCodePoint = ReplacementCharacter;
		++offset;
		return true;
	}

	// 필요한 바이트 수가 남아 있는지 확인
	// 예: 첫 바이트가 3바이트 문자라고 표시했는데 2바이트만 남았다면,
	// 문자열이 중간에 잘린 것.
	// 남은 길이를 확인해서 문자열 범위 밖을 읽지 않도록 한다.
	// text.size() - offset => 현재 위치부터 문자열 끝까지 몇 바이트

	if (text.size() - offset < byteCount)
	{
		//  데이터가 잘림 � 반환
		outCodePoint = ReplacementCharacter;
		++offset;
		return true;
	}
	//  후속 바이트 검사 계속
	for (size_t i = 1; i < byteCount; ++i)
	{
		const uint8 next = static_cast<uint8>(text[offset + i]);

		// 후속 바이트는 반드시 10xxxxxx 형태여야 함
		if ((next & 0xC0) != 0x80) // 올바른 후속 바이트의 앞 두 비트
		{
			outCodePoint = ReplacementCharacter;
			++offset;
			return true;
		}
		// codePoint << 6: 기존 비트를 왼쪽으로 밀어 뒤에 6비트 공간 확보
		// next & 0x3F: 후속 바이트의 10 표시를 제거하고 하위 6비트 추출
		codePoint =	(codePoint << 6) | (next & 0x3F);
	}
	// 완성한 값이 유효한지 확인
	// 불필요하게 긴 인코딩, surrogate, Unicode 범위 초과 거부
	// 0xD800~0xDFFF: UTF-16의 surrogate 예약 영역으로,
	// 독립된 UTF-8 문자 값으로 사용 불가
	if (codePoint < minimumCodePoint
		|| codePoint > 0x10FFFF
		|| (codePoint >= 0xD800 && codePoint <= 0xDFFF))
	{
		outCodePoint = ReplacementCharacter;
		++offset;
		return true;
	}
	// 정상적인 문자 반환
	offset += byteCount;
	outCodePoint = codePoint;
	return true;
}
