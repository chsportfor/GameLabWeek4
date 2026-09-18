# FRenderAssets 제거와 AssetManager 직접 사용

WEEK3의 `FAssetManager.h/.cpp`, `LaunchEngineLoop.cpp`, `PrimitiveComponent.cpp`를 참조했다. 엔진 루프가 매니저를 소유하고, 이름으로 등록한 에셋을 매니저에서 직접 조회하는 구조로 정리했다.

```text
FEngineLoop.mAssetManager
  RegisterAsset(Name, Loader, Source) → 등록 정보 보관
  GetAssetAs<T>(Name, true) → 필요할 때 로드 → LoadedAssets에 보관

BeginFrame(Camera, AssetManager, SelectedActor)
  Collector.AssetManager → 컴포넌트·기즈모가 직접 조회
  전용 렌더 정보 → 각 GraphicsPipeline
```

## 변경한 책임

- `FRenderAssets.h/.cpp`를 삭제했다. `Meshes`, `TexturedMeshes`, `Textures`와 fullscreen/loading/default-font 보관 필드도 제거했다.
- 렌더링 파이프라인의 `mAssets`, `InitializeLoadingScreen`, `InitializeAssets`를 제거했다. 매니저 소유와 에셋 등록·정리는 엔진 루프에서 담당한다.
- `Engine/InitializeAssets.h/.cpp`는 시작 시 호출하는 두 등록 함수만 제공한다. 맵·캐시·에셋 필드를 가지는 클래스가 아니다. 소스 배열과 파일 경로를 매니저에 등록한다.
- `Rendering/BuiltinAssetNames.h`는 기본 에셋 이름과 프리미티브→이름 대응만 정의한다. 객체를 로드하거나 보관하지 않는다.
- 렌더 정보에 필요한 공유 참조는 실제 출력이 끝날 때까지 유효하다. 이름표 컴포넌트의 폰트 참조와 기존 ObjectFactory의 기본 폰트 설정도 유지했다. 이들 참조가 남아 있으면 매니저에서 언로드해도 해당 폰트는 살아 있다.

## WEEK3에서 가져온 관리 방식

| API | 동작 |
|---|---|
| `RegisterAsset(Name, Loader, Source)` | 로드 방법을 등록한다. 아직 GPU 에셋을 만들지 않는다. 같은 이름의 재등록은 기존 등록을 유지한다. |
| `RegisterAsset(Asset)` | 이미 만들어진 에셋을 등록한다. 로더·소스가 없어 언로드 후 자동 재생성은 할 수 없다. |
| `LoadAsset(Name)` | 로드된 인스턴스를 재사용하거나 등록된 소스로 생성한다. |
| `GetAsset(Name, LoadIfNotLoaded)` | 조회한다. 두 번째 인자가 true이면 미로드 에셋을 로드한다. |
| `GetAssetAs<T>` | 조회 결과의 UObject 타입을 검사한 뒤 반환한다. 타입 불일치는 nullptr다. |
| `UnloadAsset(Name)` | 로드된 에셋에 대한 매니저 참조만 해제한다. 등록 정보는 유지한다. |
| `UnregisterAsset(Name)` | 언로드하고 등록 정보도 제거한다. |
| `ForEachMetaInfo` | 에셋 객체를 로드하지 않고 등록 정보를 순회한다. |
| `Clear()` | 로드된 에셋 참조와 등록 정보를 모두 정리한다. |

등록 정보 맵에는 이름·로더·소스가, `LoadedAssets`에는 실제 에셋이 들어간다. 이전처럼 동일한 메시·텍스처 객체를 두 계층의 맵에 중복 보관하지 않는다.

WEEK4의 UObject 에셋과 타입 검사를 유지했다. WEEK3의 전역 `Get()` 대신 Collector로 매니저를 전달하므로 EngineLib에서 EngineApp의 전역 루프에 의존하지 않는다. 매니저 자체는 WEEK3와 같은 일반 `FAssetManager`이며, 이번 변경에 UObject GC 도입이나 매니저의 UObject 전환은 포함하지 않았다.

## 메시 통합

기본 큐브·구는 텍스처 여부와 관계없이 같은 `UStaticMeshAsset`을 사용한다. `Texture` 핸들로 텍스처 출력 여부를 결정한다. `Mesh.Cube.Textured`, `Mesh.Sphere.Textured` 등록과 `TexturedPrimitives.h`를 제거했다.

큐브는 기존 정점색·위치·법선·인덱스를 유지하면서 텍스처용 아틀라스 UV를 단일 `Cube_vertices` 배열에 반영했다. 구는 기존 UV가 포함된 `Sphere_vertices`를 사용한다. 런타임에서 정점·인덱스나 UV를 생성하는 코드는 추가하지 않았다.

## 수명과 검증

등록된 파일 소스와 메시 로더는 각각 FileManager와 Renderer를 참조하므로 두 객체보다 먼저 매니저를 정리한다. 현재 앱 종료 순서는 월드/컴포넌트 파괴 → 기본 폰트 참조 해제 → AssetManager.Clear → FileManager 및 렌더링 파이프라인 정리다.

Unload는 사용 중인 에셋을 강제로 파괴하지 않는다. 다른 공유 참조가 남아 있으면 살아 있고, 마지막 참조가 없어지면 소멸한다. 이 방식은 GC가 아닌 공유 소유권이다. `UObject::Destroy()`의 직접 delete를 에셋에 호출해서는 안 된다.

Debug/x64 빌드와 간소화 GPU 테스트로 등록/지연 로드, 타입 검사, 중복 등록, 언로드 후 마지막 참조 해제, 재로드, 등록 해제, 텍스처 토글 시 같은 메시 재사용, 기존 렌더링 경로를 확인했다. 전체 단위 테스트 및 실제 창의 수동 시각 검증은 실행하지 않았다.

로그: `Saved/renderer-merge-build.log`, `Saved/asset-manager-direct-quick.log`.
