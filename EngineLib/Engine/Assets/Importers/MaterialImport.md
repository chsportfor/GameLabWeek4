# 머티리얼 임포트

`FMaterialImporter`는 기존 텍스처 애셋과 색상에서 머티리얼을 만드는 경로, MTL 파일에서 여러 머티리얼 및 텍스처 애셋을 만드는 경로를 제공한다. 직렬화는 애셋별 명시적인 함수이며 리플렉션을 사용하지 않는다.

## 입력 1: 텍스처 .uasset 경로와 색상

```cpp
#include "Engine/Assets/Importers/MaterialImporter.h"

bool Success = FMaterialImporter::ImportUMaterial(
    "Textures/Brick.uasset",
    FLinearColor{1.0f, 0.8f, 0.7f, 1.0f},
    "Materials/Brick.uasset",
    true); // bStandalone

// 텍스처 없는 단색 머티리얼도 지원.
FMaterialImporter::ImportUMaterial(
    {}, FLinearColor{1, 0, 0, 1}, "Materials/Red.uasset");
```

상대 경로는 `FFileManager`의 애셋 루트 기준이다. 절대 경로도 루트 안에 있으면 받지만 파일에는 루트 기준 상대 경로로 저장한다. 출력은 `.uasset` 파일 경로를 명시해야 한다. 기존 출력 파일은 덮어쓰지 않는다.

입력 텍스처 파일의 공통 헤더를 `AssetFile::ReadHeader()`로 읽어 `UTexture2D`인지 검사한다. 이미 임포트된 애셋을 참조하므로 여기서는 DDS 디코딩이나 GPU 재검증을 하지 않는다. 따라서 이 오버로드에는 렌더러 인수가 없다. 비어 있는 경로는 텍스처 없음으로 저장한다.

## 입력 2: MTL 파일

```cpp
FMaterialImporter::ImportUMaterial(
    Renderer, L"C:/Models/Car/car.mtl");

// 기본 목적지 대신 디렉터리를 지정할 수도 있다.
FMaterialImporter::ImportUMaterial(
    Renderer, L"C:/Models/Car/car.mtl", "Materials/ImportedCar", true);
```

기본 머티리얼 출력 디렉터리는 `Assets/Materials`이며 MTL 파일명으로 하위 폴더를 만들지 않는다. MTL로부터 생성하는 텍스처는 `Assets/Textures`에 저장한다. 머티리얼 목적지를 따로 지정해도 텍스처 목적지는 `Assets/Textures`다. OBJ 일괄 임포트는 최종 메시 애셋 이름의 부산물 폴더 하나에 머티리얼과 텍스처를 함께 저장한다.

기본 출력 구조는 다음과 같다.

```text
Assets/
  Materials/
    Body.uasset
    Glass.uasset
  Textures/
    CarColor.uasset
```

- 각 `newmtl` 이름으로 머티리얼 `.uasset` 하나를 만든다.
- `Kd`는 `DiffuseColor.R/G/B`에 저장한다.
- 알파는 `d`가 있으면 `d`, 그렇지 않고 `Tr`가 있으면 `1 - Tr`, 둘 다 없으면 파서의 기본 알파 1을 사용한다.
- `map_Kd` 원본 이미지는 기존 텍스처 임포터의 변환·GPU 검증 경로를 사용한다. 새 텍스처는 `bStandalone=false`, 머티리얼은 호출 인수의 Standalone 값(기본 true)을 갖는다.
- 한 MTL에서 동일한 실제 이미지 파일을 여러 번 참조하면 텍스처 하나만 생성한다. 각 머티리얼 본문과 헤더는 같은 `.uasset` 경로를 참조한다.
- `map_Kd`가 이미 존재하는 텍스처 `.uasset`을 가리키면 새 텍스처를 만들지 않고 참조한다. 기존 텍스처의 Standalone 값은 변경하지 않는다.
- `map_Kd`가 없으면 색상만 가진 머티리얼이다.

현재 런타임 `UMaterial`이 가진 속성에 맞춰 DiffuseColor와 DiffuseTexture만 저장한다. 파서가 읽는 `Ka`, `Ks`, `Ns`, `map_d`, 노멀맵 등은 아직 머티리얼 파일에 반영하지 않는다. `map_Kd`의 옵션은 기존 파서가 경로를 추출하는 데 사용하지만 UV 변환 등의 런타임 속성으로 저장하지 않는다.

출력 충돌은 자동 덮어쓰기나 자동 이름 변경 없이 실패 처리한다. 서로 다른 원본 이미지가 같은 stem을 가져 같은 텍스처 출력 경로가 되는 경우, 대소문자만 다른 머티리얼 이름인 경우도 충돌로 처리한다. 기존 출력 텍스처를 이름만 보고 다른 원본의 결과라고 추정하여 재사용하지 않는다. 다른 임포트 결과의 텍스처를 공유하려면 그 `.uasset` 경로를 명시하면 된다.

## 재사용한 기존 API

```cpp
TArray<FObjMaterial> Materials;
FString Error;
bool Parsed = FObjImporter::LoadMaterialsFromFile(
    MtlPath, FFileManager::Get(), Materials, Error);
```

이 함수는 `Engine/Assets/ObjImporter.h/.cpp`에 이미 공개되어 있었다. MTL 텍스트 파싱을 새로 작성하지 않고 이 API를 그대로 호출한다.

내부 `ParseMtl()`은 `newmtl`, `Kd`, `d/Tr` 등을 읽고, `ParseMaterialTexturePath()`는 텍스처 옵션과 인용부호를 처리한 뒤 **MTL 파일이 있는 디렉터리를 기준으로 이미지 경로를 해석**한다. 두 내부 함수는 비공개로 유지하며 직접 호출하지 않는다. 결과의 `FObjMaterial::Name`, `DiffuseColor`, `Dissolve`, `Transparency`, `DiffuseTexturePath`를 사용한다.

## 파일 구조

정의와 직렬화 함수는 `Core/AssetSystem/AssetFile/MaterialAssetFile.h/.cpp`에 있다.

```text
고정 영역: UAJS / ContainerVersion=1 / 헤더 JSON 길이 / 본문 JSON 길이
공통 헤더 JSON: AssetType="UMaterial", SchemaVersion=1, Standalone, Dependencies
본문 JSON: DiffuseColor=[R,G,B,A], DiffuseTexture="Textures/CarColor.uasset" 또는 null
바이너리 영역: 없음
```

누락된 DiffuseColor는 흰색, 누락/null/빈 DiffuseTexture는 텍스처 없음으로 읽는다.
정확한 전체 형식은 `MaterialAssetFile.h`의 주석을 참고한다.

헤더 목록은 의존성 조사용이고 본문 경로는 diffuse 슬롯의 의미를 갖는다. `AssetFile::Serialize(FMaterial_uasset)`는 본문 경로에서 헤더 목록을 자동 생성한다. 경로가 비어 있으면 의존성도 빈 배열이다. 역직렬화는 두 정보의 일치도 검사한다.

공통 헤더 쓰기는 새 `AssetFile::SerializeHeader()`로 분리했으며, 기존 텍스처 직렬화도 같은 함수를 사용한다. 본문 읽기·쓰기는 여전히 각 애셋별 일반 함수다.

## 저장과 실패 정리

MTL 임포트는 모든 이미지 변환·GPU 검증·머티리얼 직렬화를 먼저 메모리에서 끝낸다. 그 후 `FAssetImporter::WriteImportedAssets()`가 모든 결과 파일을 기록한다. 중간 파일 기록이 실패하면 이번 호출에서 만든 모든 애셋 파일·임시 파일·빈 디렉터리를 정리한다. 기존 파일은 보존한다. 실패는 `UE_LOG`와 `false`로 보고한다.

접근 범위는 다음과 같다.

- 외부 호출: `FMaterialImporter::ImportUMaterial()` 두 오버로드.
- 임포터 공통 저장: `FAssetImporter`의 protected `WriteImportedAsset()` / `WriteImportedAssets()`.
- 텍스처 메모리 준비: private `FTexture2DImporter::PrepareTexture2D()`. 필요한 임포터에만 friend로 허용하여 외부에 중간 준비 API를 노출하지 않는다.

단일 스레드 호출을 전제로 한다. 일반적인 실패는 롤백하지만, 프로세스 강제 종료까지 복구하는 다중 파일 트랜잭션은 아니다.

## 로드

```cpp
Assets.ScanAssets(); // 머티리얼과 텍스처의 헤더 등록
UMaterial* Material = Assets.GetAssetAs<UMaterial>("Materials/Brick.uasset", true);
```

매니저가 생성한 `UMaterial`의 가상 `Load()`가 본문을 역직렬화한다. DiffuseTexturePath가 있으면
등록된 `UTexture2D`를 매니저로 로드해 `DiffuseTexture`에 연결한다. 같은 경로는 같은 객체를 공유한다.
지정한 텍스처 애셋을 찾지 못하면 GetAssetAs가 기본 텍스처를 반환한다. 참조 경로 자체가 비어 있으면 nullptr을 유지한다. 타입 불일치, 손상된 데이터,
기본 텍스처까지 없는 경우에는 로드가 실패한다. 씬 참조 복원에도 기본 애셋 대체가 적용된다.
MTL은 임포터만 읽는다. `FMaterialAssetLoader`, `FMaterialAssetSource`, 라이브러리 파일은 제거했다.

## 검증

`python Tools/run_asset_checks.py`가 OBJ/MTL에서 만든 머티리얼·텍스처 로드 및 공유,
Standalone 보존, 마지막 참조 제거 후 텍스처 파일 삭제, 기본 머티리얼 대체를 검사한다.
