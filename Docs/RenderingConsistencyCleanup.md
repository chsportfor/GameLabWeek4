# 렌더링 구조 일관성 정리

앱의 프레임 루프, 월드/액터/컴포넌트, 에디터와 피킹, 렌더링 pass, 에셋 및 수학 코드의 호출 관계와 프로젝트 등록을 확인했다. 현재 설계의 기준은 **컴포넌트가 전용 Collector 배열을 작성하고, 각 pass가 해당 배열을 처리한다**는 것이다. 예전 관리자 API를 유지하기 위한 재분류·변환·상태 복사를 제거했다. 현재 전체 흐름은 [TypedRenderCollector.md](TypedRenderCollector.md)를 따른다.

## 책임을 옮긴 부분

| 이전 구조 | 현재 구조와 이유 |
|---|---|
| `Render(Collector, Camera, SelectedActor)`와 관리자 내부 카메라·행렬 캐시 | `BeginFrame(Camera, SelectedActor)`에서 `Collector.View`를 만들고 `Render(Collector)`가 사용한다. 제출 시점과 출력 시점의 카메라가 달라지는 중복 경로를 없앴다. |
| 공개 `Prepare`와 별도 프레임 상태 갱신 | 프레임 시작은 `BeginFrame`으로 통합했다. `URenderer::Prepare`는 실제 렌더 타깃 초기화에 필요하므로 유지한다. |
| 각 pass에 행렬·뷰포트·카메라 축을 서로 다른 인자 형태로 전달 | `FRenderView`를 const 참조로 공유한다. fullscreen은 카메라를 쓰지 않아 제외한다. |
| 관리자가 텍스트 정보를 하나씩 `Draw` | `FTextGraphicsPipeline::Draw(TextInfos, View)`가 순회하고 검증·버퍼 업로드·출력을 담당한다. 잘못된 항목 하나가 나머지 텍스트를 중단시키지 않는다. |
| 로딩 화면에 메시·텍스처를 개별 인자로 전달하고 내부에서 Present | `FRenderFullscreenInfo` 배열로 출력하고, 앱이 일반 프레임처럼 `Display`를 호출한다. |
| Collector의 `BoundsInfos`를 관리자가 선분으로 변환 | 선택된 액터의 컴포넌트만 표시 설정과 컬링을 확인하고 `AddBounds`로 `LineInfos`에 직접 작성한다. `Render`의 선분 추가 단계와 중간 배열을 제거했다. |
| Collector의 `PickInfos`에서 선택 메시를 다시 구성 | 선택된 액터의 각 프리미티브 컴포넌트가 `SelectionInfos`를 작성한다. `FPickInfo`는 에디터 CPU 피킹에만 사용한다. |
| 관리자가 선택 테두리용 확장 행렬을 생성 | `FStencilOutlineGraphicsPipeline`이 프레임 정보로 화면상 약 3px 확장을 계산한다. mark/outline은 같은 원본 메시 배열을 받는다. |
| 관리자가 투명 쿼드를 정렬 | `FQuadGraphicsPipeline`이 투명 단계에서 카메라 깊이로 뒤에서 앞으로 안정 정렬한다. 같은 깊이는 제출 순서를 유지한다. |
| Render 안에서 월드 축·그리드 정보 생성 | `BeginFrame`이 표시 설정으로 전용 배열을 준비한다. Render는 WorldAxis/WorldGrid pass에 그대로 전달한다. |
| 각 pass의 `FRenderInfo` 별칭 | 함수 선언에 실제 전용 타입을 표기한다. |
| Mesh/StencilMark/StencilOutline의 동일한 셰이더 상수 구조체 | `FMeshShaderConstants.h` 하나로 공유한다. 동일 셰이더의 메모리 배치가 서로 달라질 위험을 줄였다. |
| 빌보드·이름표·outline의 중복 스케일 추출 | 행 벡터 규약에 맞게 `FMatrix::GetScale()`을 고치고 공통 사용한다. 회전과 비균일 스케일을 함께 준 경우로 확인했다. |

## 삭제한 API·상태

| 항목 | 이전 역할 / 삭제 이유 |
|---|---|
| `FRenderingPipeline`의 중복 view/projection/camera/viewport 상태 | 프레임 카메라 캐시. `FRenderView`로 대체했다. |
| `GetAspect`, `GetViewProjectionMatrix`, `SetPerspectiveRatio` | 호출처 없는 관리자 접근자. 실제 사용 중인 투영 전환 API는 유지했다. |
| Collector `BoundsInfos`, `PickInfos` | 중간 변환과 선택 검색용 배열. 선분·선택 메시 직접 제출과 별도 CPU 피킹으로 대체했다. |
| `FCamera::GetProjectionMatrix`, `GetProjectionT_pMatrix`, `GetProjectionT_oMatrix`, `GetProjectionT_uMatrix`, `GetProjectionSMatrix`, `GetOrthographicMatrix` | 현재 호출되지 않는 분리 투영 경로. 실제 사용하는 unified 투영·역투영 경로를 유지했다. |
| `FCamera::Update` 선언, `mOrthoHeight` | 구현 없는 함수 선언과 사용되지 않는 이전 직교 설정. 실제 투영은 `mOrthoDistance`를 사용한다. |
| `FEditorViewportClient::DeprojectScreenToWorld`, `DeprojectScreenToWorldForOrtho` | 사용되지 않는 구형 역투영 경로. unified 경로와 공통 Camera near/far 값만 사용한다. |
| 에디터의 빌보드 전용 월드 바운드 재계산 | 제출된 피킹 정보가 이미 실제 빌보드 행렬의 월드 바운드를 가지고 있어 중복이었다. |
| `USceneComponent::BoundingBox`, `UPrimitiveComponent::mWocalBounds` | 사용되지 않는 바운드 저장소. 실제 로컬 바운드와 변환으로 계산한다. |
| 구의 `mSubUVMesh` | 회전각으로 다시 계산 가능한 UV 중간 상태. 제출 시 UVOffset을 작성한다. |
| 주석 속 World/Actor Render, 이전 클릭 처리, FGuiInputField 및 raw texture 코드 | 실행되지 않는 이전 경로 설명이 현재 API와 충돌해 제거했다. 유효한 설계 설명과 실제 미구현 과제는 유지했다. |

## 삭제한 파일

이 표는 이번 일관성 정리에서 추가로 삭제한 파일이다. 앞서 삭제한 GraphicsManager·레거시 셰이더 목록은 [RenderingPipelineMigration.md](RenderingPipelineMigration.md)에 있다.

| 파일 | 역할과 대체 |
|---|---|
| `EngineLib/Rendering/SubUVMesh.h`, `.cpp` | UV 상태용 래퍼. 전용 렌더 정보의 UV 값으로 대체했다. |
| `EngineLib/Rendering/Camera.cpp` | 헤더 include만 있던 빈 구현 파일. 카메라 구현은 헤더에 있다. |
| `EngineLib/Rendering/Primitives/Primitive.h` | 사용되지 않는 이전 Primitive/Sphere/Cube 클래스 계층. 현재 baked 정점·인덱스와 정적 메시 에셋을 사용한다. |
| `EngineLib/Core/Math/Projection.h`, `RayCast.h` | 구현과 사용처가 없는 빈 헤더. 실제 투영·레이캐스트 코드와 무관하다. |
| `EngineLib/Test11.h` | 예제 테스트의 `ReturnTrue` 함수. 해당 테스트 파일의 로컬 함수로 이동했다. |

추가된 `RenderView.h`, `pipelines/FMeshShaderConstants.h`와 삭제 파일을 Visual Studio 프로젝트 및 Rendering/pipelines 필터에 반영했다.

## 유지한 차이와 검증 범위

- `InstancedMeshInfos`는 인스턴싱을 위해 메시별로 묶고 `MeshInfos`는 일반 메시 출력에 사용한다. 같은 페이로드를 사용하는 서로 다른 실행 방식이다.
- 쿼드만 배열의 처리된 항목을 제거한다. 불투명·투명·overlay 단계가 한 배열을 나누어 처리하기 때문이다. 다른 pass는 정렬하거나 읽는다. 호출자는 프레임마다 새 Collector를 사용한다.
- `FTextMesh`와 `FontResource`는 폰트 에셋의 텍스트 배치·글리프 데이터에 사용되므로 유지했다. ImGui의 폰트·렌더러 경로도 에디터 UI의 별도 출력 경로다.
- CPU 피킹은 GPU 에셋이나 렌더 표시 플래그에 의존하지 않는다. 자체 프리미티브 바운드와 정점 데이터 사용은 유효하다.
- WorldAxis/WorldGrid 셰이더는 WEEK3 형태를 그대로 사용한다. 이번 정리를 위해 셰이더를 변경하지 않았다. 그리드의 X/Y축 강조색은 그리드 셰이더 자체의 표현이다.
- Debug/x64 솔루션 빌드와 기본 `Tests/run_renderer_merge.py`의 WARP/D3D11 검증을 사용한다. 전체 단위 테스트와 `--full`, 실제 앱 창의 수동 시각 검증은 이번 범위에 포함하지 않는다.

검증 로그: `Saved/renderer-merge-build.log`, `Saved/render-consistency-quick.log`.
