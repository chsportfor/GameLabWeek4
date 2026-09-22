# Texture2D 임포트

단일 폰트 아틀라스 `.uasset` 임포트는 [FontAtlasImport.md](FontAtlasImport.md)를 참고한다.

머티리얼 입력과 MTL 일괄 임포트는 [MaterialImport.md](MaterialImport.md)를 참고한다.
OBJ 및 동료의 `.pmesh` 캐시를 스태틱 메시 `.uasset`으로 변환하는 API는 [StaticMeshImport.md](StaticMeshImport.md)를 참고한다.

## 의존성

DirectXTK와 DirectXTex는 같은 vcpkg `x64-windows` 설치 및 MSBuild 사용자 통합을 사용한다.
현재 환경의 설치 위치는 `C:\vcpkg`이다. 새 개발 환경에서는 다음을 실행한다.

```powershell
C:\vcpkg\vcpkg.exe install directxtk:x64-windows directxtex:x64-windows
C:\vcpkg\vcpkg.exe integrate install
.\premake5.exe vs2026
```

- DirectXTK: `<directxtk/DDSTextureLoader.h>`, `DirectXTK.lib` — 렌더러의 GPU 생성.
- DirectXTex: `<DirectXTex.h>`, `DirectXTex.lib` — 임포터의 CPU 이미지 변환.
- DirectXTex 헤더는 텍스처 변환과 빌트인 파일 생성 도구에서 사용한다.
- `premake5.lua`의 공통 링크 설정에 DirectXTex를 추가했다. 기존 NuGet 경로 설정이 남아 있어도 현재 환경에서는 vcpkg 사용자 통합을 통해 실제 헤더·라이브러리를 찾는다.

## 호출

`UTexture2D::ImportUTexture2D()` 자리는 제거하고 별도 임포터로 이동했다.

```cpp
#include "Engine/Assets/Importers/Texture2DImporter.h"

// FFileManager는 엔진 초기화에서 Initialize(), Renderer는 디바이스 생성이 끝난 상태.
FTexture2DImporter::ImportUTexture2D(Renderer, L"C:/Images/Brick.png");
// 기본 출력: <애셋 루트>/Textures/Brick.uasset

FTexture2DImporter::ImportUTexture2D(
    Renderer, L"C:/Images/Brick.png", "Materials/Brick/Albedo.uasset", false);
// 특정 출력 파일 지정, bStandalone=false.
```

인수는 렌더러, 입력 파일, 출력 경로, Standalone 순서다. 출력 경로 기본값은 `Textures`, Standalone 기본값은 `true`이다. 상대 경로는 `FFileManager`의 애셋 루트를 기준으로 한다. 기본 엔진 실행 위치에서는 `EngineLib/Assets`가 루트다.

출력 경로에 `.uasset` 확장자가 있으면 파일 경로로 사용하고, 그렇지 않으면 디렉터리로 취급하여 `<입력 파일 stem>.uasset`을 붙인다. 출력은 애셋 루트 안으로 제한하며, 없는 디렉터리는 만든다. 같은 파일이 이미 있으면 실패하여 기존 파일을 보존한다.

## 실행 흐름

```text
FTexture2DImporter::ImportUTexture2D
  ├─ FFileManager: 원본 파일 읽기
  ├─ DDS 시그니처가 있으면 원본 바이트 보존
  └─ 그 외 WIC 지원 이미지
       LoadFromWICMemory → 필요 시 RGBA8 Convert
       → GenerateMipMaps(levels=0) → SaveToDDSMemory
  ↓
URenderer::CreateTexture2DFromMemory: 실제 GPU 텍스처 생성 검증
  ↓ 단일 2D 텍스처 확인 후 ComPtr 스코프 종료로 해제
AssetFile::Serialize: 공통 헤더 + DDS 본문 직렬화
  ↓
FAssetImporter::WriteImportedAsset
  → 임시 파일 쓰기 → 최종 파일로 이동
  → FAssetImporter::UpdateAssetLibrary
```

비 DDS 이미지는 비압축 `R8G8B8A8_UNORM`으로 통일하고, 1×1까지 전체 밉 체인을 만든다. 1×1 이미지는 이미 전체 체인이므로 밉 생성 단계를 건너뛴다. 기존 WIC 로드와 같이 sRGB 메타데이터는 무시한다. BC 압축, HDR 보존, 노멀맵 전용 밉 필터 등은 수행하지 않는다. 여러 프레임의 이미지는 첫 프레임만 사용한다.

DDS 입력은 압축 포맷·밉맵을 변경하지 않는다. GPU 생성에 실패하거나 배열·큐브맵이면 임포트에 실패한다. 검증용 GPU 텍스처는 파일을 쓰기 전에 해제하며, 애셋매니저에 임시 UTexture2D를 등록하지 않는다.

## 파일 정의의 위치와 형식

파일 표현은 런타임 UObject 및 임포터 양쪽에서 사용할 수 있도록 `Core/AssetSystem/AssetFile`에 둔다.

- `AssetFile.h`: `FFile_uasset` — 클래스 이름, Standalone, 의존 애셋 경로.
- `AssetFile.cpp`: `AssetFile::ReadHeader(std::istream&)` — 파일 스트림에서 공통 헤더만 읽기. 버퍼용 `ReadHeader(std::span<const uint8>&)`도 같은 해석 코드를 사용한다.
- `Texture2DAssetFile.h`: `FTexture2D_uasset` — 공통 정보와 DDS 바이트 배열.
- `Texture2DAssetFile.cpp`: `AssetFile::Serialize`, `DeserializeTexture2D`.

이 구조체들은 메모리에서 사용하는 표현이며, 구조체 메모리를 통째로 디스크에 쓰지 않는다. 자체 `.uasset` 형식으로 Unreal의 패키지 포맷과는 별개다.

버전 1 파일은 다음 순서로 기록한다. 정수는 little endian이다.

| 필드 | 디스크 표현 |
|---|---|
| 시그니처 | `UAST` 4바이트 |
| 버전 | uint32, 현재 1 |
| 애셋 클래스 | uint32 UTF-8 바이트 길이 + 문자열, `UTexture2D` |
| Standalone | uint8, 0 또는 1 |
| 의존성 개수 | uint32, 현재 텍스처 임포트에서는 0 |
| 각 의존성 | uint32 UTF-8 바이트 길이 + 경로 문자열 |
| 본문 길이 | uint64 |
| 본문 | DDS 헤더 및 전체 밉 데이터를 포함한 원본 DDS 바이트 |

`TArray`의 크기 표현에 맞춰 현재 구현의 파일/본문 크기는 int32 범위로 제한한다. 읽을 때 시그니처·버전·클래스·길이·남은 바이트를 검증한다.

`UTexture2D::Load()`가 `.uasset`을 역직렬화하고 내장 DDS만 렌더러로 전달한다. 원본 DDS·PNG는 임포터의 입력이며 런타임에서 직접 로드하지 않는다.

등록 단계에서는 전체 파일을 메모리에 읽지 않고 다음처럼 헤더만 얻을 수 있다.

```cpp
std::ifstream Stream(AssetPath, std::ios::binary);
FFile_uasset Header = AssetFile::ReadHeader(Stream);
```

반환 시 스트림은 마지막 의존 경로 바로 다음, 즉 애셋별 본문 시작에 위치한다. 텍스처의 경우 다음 필드는 `uint64` DDS 본문 길이다. 헤더만 읽을 때는 본문의 존재·유효성이나 GPU 생성 가능 여부까지 검사하지 않는다. 읽기 실패, 잘못된 시그니처·버전·플래그 또는 잘린 헤더는 예외로 보고한다. `DeserializeTexture2D()`도 공통 `ReadHeader()`를 재사용한다.

## 공용 저장과 등록

`FAssetImporter`의 protected `WriteImportedAsset()` / `WriteImportedAssets()`가 파일을 저장한다.
임시 파일을 완성한 뒤 최종 이름으로 이동하며 기존 파일을 덮어쓰지 않는다. 중간 실패 시 이번 호출에서
만든 파일과 빈 디렉터리를 정리하고 `UE_LOG`와 `false`로 보고한다. 프로세스 강제 종료까지 복구하는
다중 파일 트랜잭션은 아니다.

`cachelibrary`와 갱신 함수는 제거했다. 엔진 시작 시 `UAssetManager::ScanAssets()`가 Assets 아래의
`.uasset` 헤더를 조사한다. 실행 중 임포트한 파일을 이용하려면 임포트 성공 후 다시 `ScanAssets()`를
호출한다. 개별 `RegisterAsset(Path)`는 해당 파일의 헤더만 등록하므로 의존 파일들도 등록되어 있어야 한다.
전체 구조와 삭제·기본 애셋 정책은 [AssetSystem.md](../../../Core/AssetSystem/AssetSystem.md)를 참고한다.

## 검증

저장소 루트에서 `python Tools/run_asset_checks.py`를 실행하면 네 종류 애셋의 파일 기반 로드,
OBJ 의존성 공유, 헤더 등록, 파일 연쇄 삭제, 객체 수명과 씬 참조 대체를 WARP 디바이스로 검증한다.
