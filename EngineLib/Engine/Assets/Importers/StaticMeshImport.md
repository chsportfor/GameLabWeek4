# 스태틱 메시 파일과 OBJ 임포트

## 호출

```cpp
#include "Engine/Assets/Importers/StaticMeshImporter.h"

// Assets/StaticMeshes/Car.uasset
bool Success = FStaticMeshImporter::ImportUStaticMesh(Renderer, "Sources/Car.obj");

// 최상위 메시 파일명은 Destination으로 지정할 수 있다. 부산물은 최종 메시 파일명과 같은 이름의 폴더에 함께 저장한다.
Success = FStaticMeshImporter::ImportUStaticMesh(
    Renderer, "Sources/Car.obj", "Vehicles/SportsCar.uasset");

// 동료의 PODOMSH 버전 2 .pmesh 캐시를 입력으로 사용한다.
Success = FStaticMeshImporter::ImportUStaticMeshFromBinary(
    Renderer, "Sources/Car.pmesh", "Vehicles");
```

상대 경로의 기준은 `FFileManager::Get().GetFileDirectoryPath()`이다. 입력은 외부 절대 경로도 허용한다. 출력은 애셋 루트 안의 디렉터리 또는 `.uasset` 파일 경로이며, 기본 디렉터리는 `StaticMeshes`다. 마지막 인수 `bStandalone`은 기본값 `true`이며 최상위 메시 파일에 적용한다.

이름 지정은 별도의 이름 인수나 UI 옵션이 아니라 `Destination`에 `.uasset` 파일 경로를 주는 방식이다. 이름의 출처는 다음과 같이 구분한다.

- 최상위 메시 애셋: 기본값은 OBJ 파일의 확장자 없는 이름이며, 명시적 출력 파일 경로로 변경할 수 있다.
- 부산물을 모으는 폴더: 최종 메시 `.uasset` 파일의 확장자 없는 이름이다. 메시 파일 바로 옆에 만들고, 머티리얼과 텍스처를 하위 분류 폴더 없이 함께 넣는다.
- 머티리얼 파일: MTL의 `newmtl Body`라면 `Body.uasset`이다. OBJ의 `usemtl Body`가 그 머티리얼을 선택한다.
- 텍스처 파일: MTL의 `map_Kd images/CarDiffuse.png`라면 원본 이미지 파일명에서 확장자를 바꾼 `CarDiffuse.uasset`이다.

두 번째 호출의 결과는 다음과 같다.

```text
Assets/
  Vehicles/
    SportsCar.uasset
    SportsCar/
      Body.uasset
      Glass.uasset
      CarDiffuse.uasset
```

새로 생성하는 머티리얼과 텍스처는 `bStandalone=false`다. 같은 원본 이미지 파일을 참조하는 머티리얼들은 한 번 생성한 텍스처 애셋을 공유한다. 이미 존재하는 텍스처 `.uasset`을 MTL에서 지정한 경우에는 해당 애셋을 참조하고 복사하지 않는다. 기존 출력 파일은 덮어쓰지 않으며, 서로 다른 이미지 또는 머티리얼과 텍스처가 같은 출력 파일명을 요구하는 경우에도 기존 충돌 검사에 따라 임포트를 실패시킨다.

## 실행 흐름

```text
ImportUStaticMesh / ImportUStaticMeshFromBinary
  → FObjImporter::LoadFromFile / LoadBinaryFromFile
  → 파싱 결과 FStaticMesh
  → ImportParsedMesh
      → FVertexPNCT를 FVertexSimple로 변환
      → 섹션과 머티리얼 슬롯 순서 유지, 바운드 계산
      → FMaterialImporter::PrepareMaterials
          → 머티리얼 .uasset 준비
          → FTexture2DImporter::PrepareTexture2D
              → DDS 변환·GPU 검증 후 텍스처 .uasset 준비
      → AssetFile::Serialize(FStaticMesh_uasset): 데이터 검증·직렬화
      → URenderer로 정점/인덱스 버퍼 생성 검증 후 임시 버퍼 해제
      → WriteImportedAssets: 관련 파일을 함께 저장, 실패 시 이번 출력 롤백
```

OBJ 구문·삼각형 생성·MTL 해석은 기존 `FObjImporter::LoadFromFile`을 사용한다. 머티리얼 변환은 기존 MTL 임포터에서 분리한 비공개 `PrepareMaterials`를 재사용한다. 해당 함수는 `friend FStaticMeshImporter`에만 열었으며 디스크에 직접 쓰지 않는다. 현재 런타임 머티리얼이 지원하는 `Kd`, `d`/`Tr`, `map_Kd`를 저장한다. MTL이 없는 OBJ에도 기존 파서가 만든 기본 머티리얼 슬롯을 색상 머티리얼 `.uasset`으로 저장한다.

중간 준비가 실패하면 파일은 작성되지 않는다. 파일 작성이 실패하면 이번 호출에서 만든 파일과 빈 디렉터리를 제거한다. 이미 있던 파일은 유지한다. 공개 임포트 함수는 오류를 `UE_LOG(Error, Core, ...)`로 남기고 `false`를 반환한다. 전원 종료까지 보장하는 디스크 트랜잭션은 아니다.

## 메시 파일 형식

정의: `Core/AssetSystem/AssetFile/StaticMeshAssetFile.h`, 구현: 같은 위치의 `.cpp`.

`FStaticMesh_uasset`은 메모리 표현이다. 단일 파일에 UAJS 버전 1 공통 컨테이너,
헤더 JSON, 본문 JSON, 지오메트리 바이너리를 순서대로 기록한다.

| 위치 | 내용 |
|---|---|
| 고정 영역 | UAJS, ContainerVersion=1, 헤더/본문 JSON 바이트 길이 |
| 헤더 JSON | AssetType, SchemaVersion=1, Standalone, Dependencies |
| 본문 JSON | Bounds, Sections, MaterialPaths, Geometry의 형식·개수·오프셋·길이 |
| 바이너리 | 정점마다 float32 LE 12개(Position/Normal/Color/UV), 이어서 uint32 LE 인덱스 |

정확한 전체 형식과 검증 규칙은 `StaticMeshAssetFile.h`의 주석을 참고한다.

머티리얼 경로는 `Vehicles/SportsCar/Body.uasset`처럼 루트 상대 경로다. 헤더 의존성은 본문의 슬롯 배열에서 자동 생성한다. 본문에는 슬롯 순서와 중복 참조를 보존하고, 헤더에만 중복을 제거한다. 섹션 `MaterialIndex`는 본문의 슬롯 배열을 가리킨다. 텍스처는 머티리얼의 직접 의존성이며 메시 헤더에는 직접 나열하지 않는다.

읽기·쓰기 시 배열 길이, 유한한 정점값, 인덱스 범위, 바운드 일치, 섹션 범위와 전체 인덱스 포괄 여부, 머티리얼 슬롯 번호, 상대 경로, 헤더 의존성 일치, 남는 바이트를 검사한다. `AssetFile::ReadHeader`로 본문을 읽지 않고 메시 클래스와 의존성을 조사할 수 있다.

## `.pmesh` 호환성

`ObjImporter.cpp`의 `SaveMeshCache`가 OBJ 옆에 `.pmesh`를 작성한다. 형식은 `PODOMSH\0` 시그니처의 버전 2이며 `Core/Serialization/Archive.h`, `Platform/WindowsBinArchive.h/.cpp`로 읽고 쓴다. Develops 병합 후 자동 캐시 로드와 명시적 `LoadBinaryFromFile`은 같은 `TryLoadMeshCache`를 사용한다. 중복 구현이었던 `ObjMeshCache.cpp`는 제거했다.

이 파일은 독립 배포용 애셋이 아니라 원본 파싱 결과의 캐시다. 따라서 동료 코드와 동일하게 헤더에 기록된 OBJ·MTL 경로, 파일 크기, 수정 시각을 확인한다. 원본이 없거나 달라졌으면 임포트가 실패한다. 텍스처도 원본 이미지에서 읽어 부산물로 만든다. 성공 후 생성된 `.uasset`들은 이 원본 파일들에 의존하지 않는다.

바이너리 임포트에서 출력 디렉터리만 지정하면 메시 파일명은 캐시 본문에 기록된 원본 OBJ 이름을 사용한다. 출력 `.uasset` 파일명을 지정하면 그 이름을 사용한다. 두 경우 모두 부산물 폴더명은 최종 메시 파일명을 따른다. 명시적 바이너리 임포트는 기존 `.pmesh`를 읽어 `.uasset`으로 변환한다. 일반 OBJ 임포트는 유효한 캐시가 있으면 재사용하고, 없거나 오래되었으면 OBJ를 분석한 뒤 캐시를 작성한다. `.uasset` 작성이 실패해도 원본 옆의 파싱 캐시는 남을 수 있다.

## 런타임 로드

```cpp
Assets.ScanAssets(); // 메시 및 의존 파일들의 헤더 등록
UStaticMeshAsset* Mesh = Assets.GetAssetAs<UStaticMeshAsset>("Vehicles/SportsCar.uasset", true);
```

매니저가 헤더의 클래스 정보로 객체를 생성하고 `UStaticMeshAsset::Load()`를 호출한다.
메시 본문의 슬롯 순서대로 `UMaterial`을 로드하고, 머티리얼이 텍스처를 로드한다.
`UStaticMeshAsset::Initialize()`는 GPU 버퍼를 생성하고 CPU 지오메트리도 보관한다.

`UnloadCpuGeometry()`를 명시적으로 호출한 뒤 `LoadCpuGeometry()`로 `.uasset`에서 복원할 수 있다.
복원할 데이터는 기존 GPU 데이터의 개수·해시와 일치해야 한다. 파일 삭제 후 CPU 복원은 실패할 수 있다.
런타임의 OBJ 직접 로드와 별도 로더/소스 객체는 제거했다.

에디터 Import Obj와 OBJ 뷰어의 파일 선택은 이 임포터로 파일을 만든 뒤 매니저를 다시 스캔한다.
동일 이름이 존재하면 최상위 파일명에 번호를 붙이고 부산물 폴더에도 그 최종 이름을 사용한다.
`cachelibrary`는 쓰지 않는다.

## 검증

`python Tools/run_asset_checks.py`가 작은 OBJ의 임포트, 두 머티리얼의 텍스처 공유,
원본 경로 없이 GPU/CPU 로드, 공유 의존성 삭제와 삭제 후 CPU 재로드 실패를 검사한다.
