# 파일 기반 애셋 관리

## 목적과 역할

애셋의 원본 입력, 디스크 저장 형식, 실행 중 객체를 분리한다.

- 임포터: OBJ·MTL·이미지·폰트 데이터를 해석해 하나 이상의 `.uasset` 파일을 만든다.
- `UAssetManager`: 파일 헤더를 등록하고, 요청받은 클래스의 객체를 생성·보관한다. 파일 의존 관계와 삭제도 담당한다.
- `UAsset` 파생 클래스: 자기 `.uasset` 본문을 읽어 런타임 데이터와 GPU 리소스를 만든다.
- `F…_uasset`: 명시적 파일 직렬화를 위한 값 구조체다. GPU 포인터나 UObject 자체의 메모리를 저장하지 않는다.

`cachelibrary`는 사용하지 않는다. 에디터 시작 시 실제 Assets 디렉터리의 `.uasset` 파일을 재귀적으로 조사한다.
여기서 `.uasset`은 이 프로젝트의 `UAJS` JSON 컨테이너 버전 1 형식이며 언리얼의 패키지 파일과 호환되는 형식은 아니다.

파일은 `UAJS/ContainerVersion=1/헤더 JSON 길이/본문 JSON 길이` 고정 영역,
공통 헤더 JSON, 클래스별 본문 JSON, 선택적 바이너리 영역 순서다.
헤더에는 AssetType, SchemaVersion=1, Standalone, Dependencies를 기록한다.
최신 버전 등록은 헤더 JSON까지만 읽는다. 구버전은 본문을 갱신한 뒤 등록한다. 이미지 DDS와 정점·인덱스는 바이너리로 유지한다.
누락된 선택 속성은 기본값, 모르는 키는 무시, 존재하지만 타입이 틀린 속성은 오류로 처리한다.
정확한 전체 디스크 배치·필수/선택 필드·기본값은 `AssetFile/*AssetFile.h` 주석을 기준으로 한다.
구형 UAST 파일은 런타임에서 지원하지 않는다. 일회성 변환은
`python Tools/migrate_uasset_json.py --write`로 수행하며 원본은 `Tools/bin/UassetJsonBackup`에 보관한다.


## 애셋 스키마 갱신

`AssetFileSchema.h/.cpp`가 파일 업그레이드를 공통 처리한다. 각 런타임 애셋 cpp의
`IMPLEMENT_ASSET_FILE_SCHEMA(Class, Getter)`가 `FClassInfo*`와 파일 스키마를 연결한다.
UObject 클래스 등록과는 별도 연결이며, 실제 최신 버전·변환·의존성 생성·검증 함수는
각 `*AssetFile.cpp`의 `Get*FileSchema()`에 모여 있다. 전용 enum이나 매니저의 타입 switch는 없다.

```text
ScanAssets / RegisterAsset
  → ReadMetaInfo
    → ReadHeader: SchemaVersion을 반환, 파일은 수정하지 않음
    → 클래스의 GetSchema
    → UpgradeFile
      최신: 본문을 읽지 않고 반환
      미래 버전: 오류, 원본 유지
      과거: FAssetFileDocument(Header/Body/Payload) 읽기
          → Upgrade*ToLatest: 이전→다음 단계들을 순서대로 수행
          → 최신 버전 도달 및 클래스/Standalone 유지 확인
          → RebuildDependencies → Validate
          → .upgrade.tmp 작성·재독해 검증
          → 원본 백업 → Windows 파일 교체
    → 갱신된 헤더로 메타정보 작성
  → 전체 스캔 성공 후 메타정보/이름 목록/역참조 맵 적용
```

자동 백업 이름은 `<파일>.schema-vN.bak`이며 기존 백업이 있으면 `.1`, `.2`를 붙인다.
최신 파일은 재작성하지 않는다. 기존 `.upgrade.tmp`가 있으면 덮어쓰지 않고 실패한다.
이 호출이 만든 임시 파일만 실패 시 제거한다. 파일을 교체하기 전 실패하면 원본은 유지된다.
앞 파일을 정상 갱신한 뒤 다른 파일에서 스캔이 실패하면, 이미 갱신된 파일은 유지하고
기존 매니저 인덱스를 보존한다. 다음 스캔은 최신 파일을 건너뛰므로 이어서 처리할 수 있다.
개별 RegisterAsset도 같은 업그레이드를 수행한다. LoadAsset의 재확인은 읽기 전용이며,
등록 후 버전이 바뀌었다면 재등록/스캔을 요구한다. 로드된 객체의 자동 재로드는 수행하지 않는다.

현재 실제 스키마는 네 종류 모두 1이며 업그레이드 함수는 아무것도 변경하지 않는다.
함수 안의 주석은 미래 v2 예시일 뿐 실제 속성이나 버전 2를 추가한 것이 아니다.
새 형식을 도입할 때는 다음 네 곳을 함께 수정한다.

1. `Get*FileSchema()`의 LatestVersion.
2. `Upgrade*ToLatest()`의 단계별 변환. 기존 JSON 키와 바이너리는 필요한 부분만 변경한다.
3. 최신 DTO/Serialize/Deserialize 및 의존성 생성 함수.
4. 해당 `*AssetFile.h` 전체 파일 형식 주석과 구버전 변환 테스트.

구형 UAST에서 UAJS로의 일회성 변환 도구와 이 체계는 별개다.
이 업그레이드 체계는 해석 가능한 UAJS 컨테이너 내부의 SchemaVersion을 관리한다.

## 등록과 로드

```cpp
// 렌더러의 디바이스와 FFileManager 초기화 이후
UAssetManager* Assets = FObjectFactory::ConstructObject<UAssetManager>(Renderer);
if (!Assets->ScanAssets()) { /* 등록 실패 처리 */ }

// 개별 파일의 헤더만 등록/갱신할 수도 있다.
Assets->RegisterAsset("StaticMeshes/Car.uasset");

// false: 이미 로드된 객체만 조회. true: 등록되어 있다면 필요할 때 로드.
auto* Mesh = Assets->GetAssetAs<UStaticMeshAsset>("StaticMeshes/Car.uasset", true);
```

`RegisterAsset`는 의존 파일의 등록까지 수행하지 않는다. 의존성을 가진 임포트 결과를 이용할 때는
`ScanAssets()`로 모든 결과 파일을 등록하는 것이 간단하다. 일반 로드는 등록된 파일만 대상으로 한다.
등록 또는 임포트 자체가 월드에 액터를 생성하지는 않는다.

호출 흐름:

```text
엔진 시작
  → ConstructObject<UAssetManager>(Renderer)
  → FBuiltinAssetImporter::ImportMissingBuiltins(Renderer)
      → 누락된 빌트인 파일 생성, 실패 시 초기화 중단
  → ScanAssets
      → 각 파일을 ifstream으로 열기
      → AssetFile::ReadHeader (본문은 읽지 않음)
      → 헤더의 클래스 이름을 FObjectFactory::GetClassInfoByName으로 해석
      → 구체적인 UAsset 파생 클래스인지 확인
      → 메타정보·클래스별 이름 목록·역참조 맵 구성

GetAssetAs<UStaticMeshAsset>(경로, true)
  → LoadedAssets에 있으면 객체의 정확한 클래스를 검사하고 반환
  → 없으면 메타정보의 AssetClass == UStaticMeshAsset::GetClass() 확인
  → LoadAsset (등록 또는 파일이 없으면 클래스별 기본 애셋 조회/로드)
      → ConstructUnInitializedObject(Meta.AssetClass)
      → SetName(애셋 루트 상대 경로)
      → 가상 UStaticMeshAsset::Load
          → DeserializeStaticMesh
          → 슬롯 순서대로 GetAssetAs<UMaterial>
              → UMaterial::Load → GetAssetAs<UTexture2D>
                  → UTexture2D::Load → 내장 DDS로 GPU 텍스처/SRV 생성
          → Initialize로 GPU 버퍼 생성, CPU 지오메트리 보관
      → LoadedAssets에 보관
```

네 종류 모두 가상 `Load(Path, Assets, Renderer)`를 구현한다. 폰트는 `UFontAtlasAsset::Load`가
내장 DDS와 내장 JSON/격자 설정을 읽으므로 별도 텍스처 애셋을 요구하지 않는다.
애셋별 Initialize는 이미 준비한 데이터로 객체를 구성하는 내부 단계로 남아 있다.
매니저 이용자는 Initialize를 직접 선택·호출할 필요가 없다.

`EAssetType`, `TAssetType`, `ASSET_TYPE_LIST`는 제거했다. 클래스 정보는 기존 UObject 클래스 등록을 사용한다.
`DECLARE_ASSET_TYPE(Class)`는 해당 클래스의 `GetRegisteredAssetNames()`를 제공한다.
새 클래스에는 UObject 클래스 등록, 가상 Load, 필요하면 기본 애셋 경로를 추가한다.
메타정보 맵과 이름 목록이 같은 클래스 정보를 사용하므로 별도 enum 목록이나 클래스별 include 분기가 없다.
`GetAssetAs<T>()`는 정확한 클래스만 허용한다. 폰트가 UTexture2D를 상속하더라도 일반 텍스처 목록에는 나오지 않는다.

### 메타정보와 동일 경로 재등록

`FAssetMetaInfo`는 AssetName, AssetClass, bStandalone, 중복 없는 Dependencies를 보관한다.
이름은 `StaticMeshes/Car.uasset`처럼 Assets 기준 상대 경로다. 외부로 빠지는 경로는 거부한다.
FName 비교에 따라 대소문자는 구별하지 않는다. 다른 폴더의 같은 파일명은 다른 애셋이다.

같은 경로·같은 클래스의 재등록은 헤더를 갱신하며 이름 목록에 중복 추가하지 않는다.
이미 로드한 객체의 리소스는 자동 재로드하지 않는다. 개별 RegisterAsset의 클래스 변경은 거부한다.
전체 ScanAssets에서 파일이 없어지거나 클래스가 바뀌면 기존 객체를 LoadedAssets에서 제거하고
RetiredAssets로 옮긴다. 같은 경로·같은 클래스의 파일 내용만 바뀐 경우에는 기존 객체를 유지한다.
파일 내용 변경을 즉시 반영하는 별도의 재로드 기능은 없다.

ScanAssets는 전체 헤더를 정상적으로 읽었을 때만 새 인덱스를 적용한다. 헤더 오류가 있으면 실패를 기록하고
이전 인덱스를 유지한다. 등록 성공은 본문 유효성이나 GPU 생성 성공을 보장하지 않는다.
로드 중 오류는 로그와 예외로 전달하며 부분 생성 객체는 파괴한다. 로드 순환 의존성도 거부한다.

## 파일 삭제와 객체 수명

```cpp
bool Deleted = Assets->DeleteAsset("StaticMeshes/Car.uasset");
TArray<FName> Referencers = Assets->GetReferencers("Textures/CarColor.uasset");
```

DeleteAsset은 `.uasset` 파일을 삭제하는 함수다. 원본 OBJ·이미지, 빈 폴더, 실행 중 객체는 삭제하지 않는다.
파일마다 헤더에 적힌 직접 의존성을 정방향으로 저장하고, 반대로 각 의존 파일을 참조하는 애셋 이름 집합도 만든다.
같은 파일을 여러 슬롯에서 써도 하나의 참조자로 센다.

| 검사 대상 | 파일 삭제 조건 |
|---|---|
| 호출자가 지정한 파일 | 역참조가 0. Standalone 여부는 무관 |
| 연쇄적으로 검사하는 의존 파일 | 역참조가 0이고 Standalone이 false |

실행 흐름:

1. 현재 등록 정보와 역참조 맵을 사용한다. 삭제 시 전체 디렉터리를 다시 스캔하지 않는다.
   외부 파일 변경은 초기화 또는 명시적 ScanAssets로 반영해야 한다.
2. 직접 대상의 역참조를 검사한다.
3. 파일 삭제가 성공한 뒤에만 등록 정보와 클래스별 이름 목록을 제거한다.
4. 대상의 각 의존 파일에서 역참조 하나를 제거한다.
5. 의존 파일마다 조건을 검사한다. 보존해야 하는 파일은 그 아래를 검사하지 않는다.
6. 삭제 가능한 파일에 같은 처리를 반복한다.

```text
A → B → T
  → C → T
```

A를 지우고 B를 지우면 T에는 C의 참조가 남으므로 보존한다. 이어서 C를 지울 때 T의 역참조가 0이 되고
다시 검사하여 삭제한다. 보존된 파일을 영구적인 방문 완료 집합에 넣지 않는다.
순환 참조 집합은 역참조가 남기 때문에 이 정책으로 자동 수거하지 않는다.
Standalone=false인 고아 파일을 스캔만으로 삭제하는 기능도 없다. 정리 대상은 삭제 호출에서 출발한 의존 관계다.

파일 삭제가 실패하면 그 파일의 등록·의존 관계를 유지한다. 연쇄 삭제 중 일부 실패는 false와 로그로 보고하며,
이미 성공한 파일 삭제까지 복구하는 트랜잭션은 아니다. 다른 의존 분기는 계속 처리한다.

### 로드된 객체는 유지

현재 GC 및 TObjectPtr이 없으므로 매니저가 로드된 객체를 원시 포인터로 소유하고 종료까지 유지한다.
파일 삭제가 성공하면 해당 객체를 `LoadedAssets`에서 빼고 `RetiredAssets`에 보관한다.
기존 컴포넌트·머티리얼·렌더 인포의 포인터는 유효하지만, 이름 조회에서는 이 객체를 반환하지 않는다.
같은 경로에 파일을 다시 만들고 등록·로드하면 새 객체를 생성한다. 기존 사용자에게 새 객체를 자동 연결하지 않는다.
삭제 뒤 재등록하기 전의 타입 지정 조회는 기본 애셋 대체 정책을 따른다.
`Clear()` 또는 매니저 소멸 시 활성 객체와 RetiredAssets의 모든 이전 객체를 파괴한다.
월드와 렌더 수집기의 사용을 먼저 종료해야 한다. 명시적 스캔에서 발견한 파일 삭제·클래스 변경에도 같은 보관 처리를 적용한다.

메시는 최초 로드 시 CPU 지오메트리도 보관한다. 명시적 UnloadCpuGeometry 이후 파일이 삭제되었다면
LoadCpuGeometry는 로그를 남기고 false를 반환한다. 기존 GPU 버퍼는 유지된다.
ReleaseAsset, 공유 포인터 기반 참조 수, GC는 이번 변경에 포함되지 않는다.

## 누락된 애셋 조회와 씬 참조

각 애셋 클래스가 정적 `GetDefaultAssetName()`으로 기본 파일을 지정한다.

| 클래스 | 기본 경로 |
|---|---|
| UTexture2D | Engine/Textures/White.uasset |
| UMaterial | Engine/Materials/Default.uasset |
| UStaticMeshAsset | Engine/Primitives/Cube.uasset |
| UFontAtlasAsset | Fonts/KoreanFullAtlas.uasset |

```text
컴포넌트 DeserializeClass
  → TPropertyJsonSerializer<T*>::Deserialize
  → LoadAssetReference(저장 경로, T::GetClass(), T::GetDefaultAssetName())
      → 등록된 애셋 조회/로드
      → 없다면 경고 후 기본 경로 조회/로드
      → 필드의 기대 클래스와 호환되는지 확인
  → 컴포넌트의 포인터 필드에 설정
```

JSON null은 nullptr로 유지한다. 따라서 머티리얼 오버라이드 해제 상태를 기본 머티리얼로 덮지 않는다.
애셋 포인터 배열도 같은 정책을 적용한다. 기본 파일과 로드된 기본 객체가 모두 없으면 씬 복원은 예외로 실패한다.
DeleteAsset 또는 명시적 스캔으로 기본 파일 삭제가 반영되면, 기존 기본 객체도 RetiredAssets로 이동하므로
대체 조회에 반환하지 않는다. 기존 사용자의 포인터만 유지한다. 기본 애셋을 재귀적으로 대체하지 않는다.
잘못된 클래스, 손상된 본문, GPU 생성 실패는 누락으로 위장하지 않고 실패로 전달한다.
일반 `GetAssetAs<T>(Name, true)`와 애셋 내부의 의존성 조회에도 대체를 적용한다. 메시의 머티리얼이
없으면 기본 머티리얼을, 머티리얼의 텍스처가 없으면 기본 텍스처를 연결한다. 기본 애셋마저 없으면
GetAssetAs는 nullptr를 반환하고 필수 의존성을 요구하는 로드 함수가 예외를 발생시킨다.
타입 불일치는 대체하지 않으며, 본문/GPU 오류도 그대로 전달한다.
`LoadIfNotLoaded=false`는 순수 캐시 조회로 유지한다. 없는 이름에는 기본 애셋을 로드하거나 반환하지 않는다.
빈 FName도 nullptr로 유지한다. 대체 객체를 원래 누락 경로의 캐시에 별도로 등록하지 않는다.
대체 후 씬을 저장하면 대체된 기본 경로가 저장된다. 원래 누락 경로는 별도 보존하지 않는다.

역참조 맵에는 `.uasset` 헤더만 포함된다. 저장된 `.Scene`의 참조와 실행 중 객체 포인터는 파일 삭제를 막지 않는다.

## 빌트인과 에디터 연결

빌트인도 실제 `.uasset` 파일이다. 메인 프로그램은 렌더러와 파일매니저 초기화 후
`ImportMissingBuiltins()`를 호출하고, 성공하면 `ScanAssets()`로 다른 파일과 함께 등록한다.
생성에 실패하면 임포터가 오류를 기록하고 메인 초기화도 실패한다.
원하는 경우 저장소 루트에서 다음 개발용 도구로 미리 생성할 수도 있다.

```powershell
python Tools/run_asset_checks.py --prepare-builtins
```

`FBuiltinAssetImporter::ImportMissingBuiltins`는 프리미티브 배열, 기존 DDS, 폰트 PNG/JSON으로 누락 파일만 만든다.
기존 `.uasset`은 덮어쓰지 않는다. 누락된 파일만 시작 시 생성하므로 삭제한 빌트인은 다음 실행에서 다시 생성된다.
이미지·폰트를 새로 생성하려면 해당 원본 DDS 또는 PNG/JSON이 필요하다.
프리미티브 배열은 이 생성 도구의 입력이며 별도 런타임 프리미티브 로더는 없다.

에디터 Import Obj와 OBJ 뷰어의 입력 경로는 `ImportStaticMeshObjAsset`을 통해
`FStaticMeshImporter::ImportUStaticMesh → ScanAssets → GetAssetAs`로 이어진다.
결과 파일명이 중복되면 번호를 붙이고 같은 이름의 부산물 폴더에 머티리얼·텍스처를 함께 둔다.

기존 로컬 `StaticMeshes/McQueen` 및 `McQueen_1`의 OBJ도 이번에 각각
`StaticMeshes/McQueen.uasset`, `StaticMeshes/McQueen_1.uasset`으로 변환했다.
머티리얼·텍스처 `.uasset`은 각 이름의 폴더에 있고, 기존 원본 파일은 임포트 입력으로 남아 있다.
샘플 씬의 MTL 서브항목 참조는 `StaticMeshes/McQueen/Material_030.uasset`으로 갱신했다.
명시적 개발용 변환은 다음처럼 실행할 수 있다. 기존 출력은 덮어쓰지 않는다.

```powershell
python Tools/run_asset_checks.py --import-obj StaticMeshes/McQueen/McQueen.obj StaticMeshes/McQueen.uasset
```

## 제거된 부분

| 제거 항목 | 이전 역할과 대체 |
|---|---|
| FAssetLoader, FAssetSource | 로더·입력 객체 쌍으로 등록 → 파일 경로와 UAsset::Load |
| FFileAssetSource | 파일매니저·입력 경로 보관 → 메타정보 경로와 매니저의 루트 |
| FStaticMeshAssetSource | 프리미티브 배열을 런타임 로더에 전달 → 빌트인 `.uasset` |
| FFontAtlasAssetSource | 외부 이미지·JSON/격자 입력 묶음 → 폰트 파일 본문 |
| FTexture2DAssetLoader, FFontAtlasAssetLoader | 외부 소스로 객체 생성 → 각 애셋의 Load |
| FMaterialAssetLoader, FMaterialAssetSource, RegisterMaterialLibrary | MTL 런타임 파싱·서브애셋 등록 → 머티리얼 임포트와 독립 파일 |
| FStaticMeshAssetLoader_Primitive / _File | 배열 또는 OBJ·uasset 로드 → UStaticMeshAsset::Load |
| EAssetType, TAssetType, ASSET_TYPE_LIST | 별도 타입·레지스트리 분기 → FClassInfo 기준 |
| MakeSubAssetName | MTL 경로#머티리얼 이름 → 개별 머티리얼 파일 경로 |
| RegisterAsset(UAsset*), UnregisterAsset | 직접 객체 등록 및 등록 해제 → 파일 등록과 파일 삭제의 명확한 경로 |
| 런타임 GetDefaultMaterial 생성 | 메모리에서 기본 머티리얼 등록 → 실제 기본 머티리얼 파일 |
| RegisterLoadingScreenAssets, RegisterSceneAssets, RegisterObjFileAsset | 직접 소스/로더 등록 → ScanAssets 및 OBJ 임포트 |
| .objimport와 원본 복사·MTL 재작성 경로 | 복사 원본을 실행 시 재해석 → 완성된 `.uasset` 출력 |
| UpdateAssetLibrary 및 cachelibrary 쓰기 | 별도 경로 목록 관리 → 실제 디렉터리 헤더 스캔 |

## 검증

`python Tools/run_asset_checks.py`는 별도 임시 애셋 루트와 WARP 디바이스를 사용한다.
헤더 스캔의 UObject 미생성, 중복 등록, 정확한 타입 조회, 네 클래스 로드, OBJ 의존성 공유,
역참조가 남은 파일의 삭제 거부, 공유 의존성 재검사, Standalone 보존, 파일 삭제 후 객체 수명,
같은 경로 재생성 시 새 객체·새 데이터 로드와 이전 객체 유지, 종료 시 모든 이전 객체 파괴,
명시적 스캔에서 외부 삭제 반영, CPU 재로드 실패, 의존성 누락 대체, 스캔 없는 삭제,
null/누락/타입 오류/손상된 씬 참조, 기본 애셋 누락을 검사한다.
테스트가 생성한 파일은 검사한 전용 디렉터리 안에서만 정리한다.
