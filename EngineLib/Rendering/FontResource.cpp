#include "FontResource.h"

#include "ThirdParty/Json/json.hpp"

#include <cmath>
#include <stdexcept>

// 안에 선언한 함수와 타입을 .cpp 파일 내부에서만 사용하도록 범위를 제한
namespace
{
    using FJson = json::JSON;

    const FJson& RequireField(const FJson& object, const char* key)
    {
        if (!object.hasKey(key))
        {
            throw std::runtime_error("Missing font atlas key");
        }
        return object.at(key);
    }

	// JSON 문자를 숫자로 읽기
    float ReadNumber(const FJson& object, const char* key)
    {
        const FJson& value = RequireField(object, key);
        float result = 0.0f;
        // int, float 별로 구분되므로 추가 처리
        if (value.JSONType() == FJson::Class::Integral)
            result = static_cast<float>(value.ToInt());
        else if (value.JSONType() == FJson::Class::Floating)
            result = static_cast<float>(value.ToFloat());
        else
            throw std::runtime_error("Invalid font atlas number");

        if (!std::isfinite(result)) throw std::runtime_error("Non-finite font atlas number");
        return result;
    }

    struct FBounds
    {
        float Left, Top, Right, Bottom;
    };

	// 사각형 읽기(두 종류의 사각형 모두 같은 함수로)
    FBounds ReadBounds(const FJson& value)
    {
        FBounds bounds{
            ReadNumber(value, "left"), ReadNumber(value, "top"),
            ReadNumber(value, "right"), ReadNumber(value, "bottom")
        };

		// 높이나 너비가 이상하면 잘못된 사각형
        if (bounds.Right <= bounds.Left || bounds.Bottom <= bounds.Top)
            throw std::runtime_error("Invalid font atlas bounds");
        return bounds;
    }
}



bool FFontResource::LoadUnicodeAtlasFromString(const FString& jsonText)
{
    try
    {
		// source = "{\"atlas\":{...},\"glyphs\":[...]}"
        std::string source = static_cast<std::string>(jsonText);
        // BOM이 있는 UTF-8 처리
		// BOM = Byte Order Mark
		// 맨 앞에 붙는 3바이트의 보이지 않는 표식(EF BB BF)
		//문자열의 처음 3바이트가 UTF-8 BOM과 같은지 비교
        if (source.compare(0, 3, "\xEF\xBB\xBF") == 0)
            source.erase(0, 3); // 문자열 전체가 올바른 UTF-8인지 검사하는 것은 아님
		// 빈 파일인지 확인
		// find_first_not_of :전달된 문자들 중 첫 번째로 일치하지 않는 것의 위치
		// std::string::npos => 문자열 안에서 특정 문자나 문자열을 찾지 못했을 때 반환
		if (source.find_first_not_of(" \t\r\n") == std::string::npos)
            return false;

		//SON 객체로 변환
        const FJson root = FJson::Load(source);
		// 최상위 JSON
        const FJson& atlas = RequireField(root, "atlas");
		// 형식이나 원점이 예상과 다르면 실패
        if (RequireField(atlas, "type").ToString() != "msdf"
            || RequireField(atlas, "yOrigin").ToString() != "top")
            return false;

		// 현재 한글 atlas 3468.0f
        const float atlasWidth = ReadNumber(atlas, "width");
        const float atlasHeight = ReadNumber(atlas, "height");

		const float distanceRange = ReadNumber(atlas, "distanceRange");
		// 나중에 texture.Left / atlasWidth를 하므로 0은 제외
        if (atlasWidth <= 0.0f || atlasHeight <= 0.0f || distanceRange <= 0.0f
            || atlasWidth > 16384.f || atlasHeight > 16384.f
            || std::floor(atlasWidth) != atlasWidth || std::floor(atlasHeight) != atlasHeight)
            return false;

		// 글리프 배열 확인
        const FJson& glyphs = RequireField(root, "glyphs");
        if (glyphs.JSONType() != FJson::Class::Array || glyphs.length() == 0)
            return false;

		// 임시 맵(Unicode 번호 , 글자 정보) 준비
        TMap<uint32, FCharacterInfo> characters;
        characters.Reserve(glyphs.length());


        for (const FJson& glyph : glyphs.ArrayRange())
        {
            const FJson& unicode = RequireField(glyph, "unicode");
            if (unicode.JSONType() != FJson::Class::Integral)
                return false;
			// 라이브러리의 ToInt()가 반환하는 타입에 맞춰 long
            const long codePoint = unicode.ToInt();
			// 유효한 문자 번호인지 체크
            if (codePoint < 0 || codePoint > 0x10FFFF
                || (codePoint >= 0xD800 && codePoint <= 0xDFFF))
                return false;
			// 맵의 키 타입으로 변환
            const uint32 key = static_cast<uint32>(codePoint);
			// 동일한 Unicode 번호가 나오면 false
            if (characters.Contains(key))
                return false;

            FCharacterInfo info{};
			// 다음 글자의 기준점까지 이동할 거리
            info.AdvanceX = ReadNumber(glyph, "advance");

			// 두 사각형 정보가 있는지 체크
			// planeBounds O & atlasBounds O : 사각형 생성 가능
			// planeBounds X & atlasBounds X : 사각형 없는 글리프로 처리
			// 그 이외에는 false
            const bool hasPlane = glyph.hasKey("planeBounds");
            const bool hasAtlas = glyph.hasKey("atlasBounds");
            if (hasPlane != hasAtlas)
                return false;

            // Spaces have an advance but no quad or texture coordinates.
            info.HasGeometry = hasPlane;
            if (info.HasGeometry)
            {
				// plane: 글자 기준점 주변에서 그릴 사각형, 단위(em)
				// texture: 아틀라스 이미지에서 읽을 사각형, 단위(px)
                const FBounds plane = ReadBounds(glyph.at("planeBounds"));
                const FBounds texture = ReadBounds(glyph.at("atlasBounds"));
				// 아틀라스 이미지 범위 밖을 가리키는지
                if (texture.Left < 0.0f || texture.Top < 0.0f
                    || texture.Right > atlasWidth || texture.Bottom > atlasHeight)
                    return false;

                info.U = texture.Left / atlasWidth;
                info.V = texture.Top / atlasHeight;
                info.UVWidth = (texture.Right - texture.Left) / atlasWidth;
                info.UVHeight = (texture.Bottom - texture.Top) / atlasHeight;
                info.Width = plane.Right - plane.Left;
                info.Height = plane.Bottom - plane.Top;
                info.BearingX = plane.Left;
                // JSON은 아래 방향이 양수, 텍스트 메시에서는 +Z 방향이 위쪽
                info.BearingY = -plane.Top;
            }
			// 임시 맵에 등록
            characters.Add(key, info);
        }

        // 성공한 맵으로 교체.
        mUnicodeCharacterMap = std::move(characters);
        mAtlasWidth = static_cast<uint32>(atlasWidth);
        mAtlasHeight = static_cast<uint32>(atlasHeight);
		mDistanceRange = distanceRange;
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}
