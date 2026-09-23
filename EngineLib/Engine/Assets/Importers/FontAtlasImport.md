# 폰트 아틀라스 임포트

`FFontAtlasImporter`는 이미지와 글자 배치 정보를 단일 `UFontAtlasAsset`용 `.uasset` 파일로 저장한다.
별도의 텍스처 애셋이나 원본 이미지·JSON에 대한 의존성을 만들지 않는다. 파일을 만든 뒤 원본을 제거해도 로드할 수 있다.

## 호출

```cpp
#include "Engine/Assets/Importers/FontAtlasImporter.h"

// msdf-atlas-gen의 MSDF 이미지와 JSON. 현재 파서가 지원하는 type=msdf, yOrigin=top.
FFontAtlasImporter::ImportUFontAtlas(Renderer,
    std::filesystem::absolute("Resources/Fonts/KoreanFullAtlas.png"),
    std::filesystem::absolute("Resources/Fonts/KoreanFullAtlas.json"));
// 출력: <애셋 루트>/Fonts/KoreanFullAtlas.uasset

// 비트맵 격자 아틀라스. 셀 크기·advance는 기존 비트맵 폰트와 동일한 의미다.
FBitmapFontAtlasSettings Settings;
Settings.Columns = 16;
Settings.Rows = 16;
FFontAtlasImporter::ImportUFontAtlas(Renderer,
    "Fonts/EnglishBigFontAtlas.dds", Settings, "Fonts/English.uasset", true);
```

상대 경로는 `FFileManager`의 애셋 루트 기준이다. 출력은 기본 `Fonts` 디렉터리이며,
디렉터리를 지정하면 이미지 파일명으로 `.uasset`을 만든다. 명시적인 `.uasset` 경로도 지원한다.
Standalone은 기본 true다. 이미 존재하는 출력은 덮어쓰지 않는다.

기본 한국어 폰트의 원본은 `EngineLib/Resources/Fonts/`에 둔다.
위 예시는 작업 디렉터리가 `EngineLib`인 실행 설정을 기준으로 절대 경로를 만들어 전달한다.
따라서 원본은 애셋 루트 밖에서 읽고, 생성한 `.uasset`은 기존 `Assets/Fonts/`에 저장한다.

## 파일 구성

`Core/AssetSystem/AssetFile/FontAtlasAssetFile.h`의 `FFontAtlas_uasset`이 메모리 표현이다.
`AssetFile::Serialize()`와 `DeserializeFontAtlas()`가 명시적으로 필드를 기록·복원한다.

| 순서 | 내용 |
|---|---|
| 고정 영역 | UAJS, ContainerVersion=1, 헤더/본문 JSON 바이트 길이 |
| 헤더 JSON | AssetType=UFontAtlasAsset, SchemaVersion=1, Standalone, 빈 Dependencies |
| 본문 JSON | Mode, MSDF의 Metadata 객체 또는 비트맵의 BitmapSettings, Image의 DDS 위치·길이 |
| 바이너리 | 완전한 DDS 파일 바이트 |

단일 파일이며 구체적인 전체 형식·기본값은 `FontAtlasAssetFile.h` 주석을 참고한다.
MSDF JSON은 파일 경로가 아니라 **내용 자체**다. 기존 `FFontResource` 파서를 그대로 사용하고,
글리프·atlas 설정뿐 아니라 원본의 공통 metrics 등도 보존한다. 현재 렌더러가 사용하지 않는
lineHeight·kerning 등의 항목을 저장한다고 해서 해당 렌더 기능까지 추가되는 것은 아니다.

PNG 등 WIC 입력은 RGBA8 DDS로 변환하되 폰트에는 밉맵을 자동 생성하지 않는다.
DDS 입력은 원본 바이트와 기존 밉맵을 보존한다. 이미지 변환과 GPU 검증은
`FTexture2DImporter`의 private 준비 함수를 friend로 재사용하며 텍스처 파일은 쓰지 않는다.
일반 텍스처 임포터의 전체 밉맵 생성 기본 동작은 유지한다.

MSDF JSON 형식, 글리프 및 이미지 크기 일치, 비트맵 격자 설정, 단일 2D GPU 텍스처 생성 성공을
검사한 후 공통 `WriteImportedAsset()`으로 저장한다. 누락된 출력 폴더는 생성한다.
실패 시 UE_LOG를 남기고 빈 `TArray<FName>`을 반환하며, 파일 저장 실패 시 이번 파일·빈 디렉터리를 정리한다.

## 로드

```cpp
const auto ImportedNames = FFontAtlasImporter::ImportUFontAtlas(Renderer,
    std::filesystem::absolute("Resources/Fonts/KoreanFullAtlas.png"),
    std::filesystem::absolute("Resources/Fonts/KoreanFullAtlas.json"));
for (const FName& Name : ImportedNames)
    Assets.RegisterAsset(std::filesystem::u8path(Name.ToString().CStr()));
auto* Font = Assets.GetAssetAs<UFontAtlasAsset>("Fonts/KoreanFullAtlas.uasset", true);
```

매니저가 생성한 `UFontAtlasAsset`의 가상 `Load()`가 파일을 역직렬화하고 내장 DDS로 GPU 텍스처/SRV를 만든다.
내장 JSON 또는 격자 설정으로 `FFontResource`를 구성하며 별도 `UTexture2D` 객체는 생성하지 않는다.
클래스별 이름 레지스트리에서도 폰트는 일반 텍스처 목록에 포함되지 않는다.

기본 한국어 폰트도 `Fonts/KoreanFullAtlas.uasset`에서 로드한다. 원본 PNG·JSON은 임포트할 때만 필요하다.
런타임의 `FFontAtlasAssetSource`와 `FFontAtlasAssetLoader`는 제거했다.
