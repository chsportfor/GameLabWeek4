# 컴포넌트별 렌더 제출 구조

현재 실행 경로는 다음과 같다. `RenderingPipelineMigration.md`의 관리자 전환 기록 이후, 범용 제출 경로까지 교체한 최종 구조이다.

```text
Actor/Component.Update(deltaTime)                  상태 갱신
RenderingPipeline.BeginFrame(Camera, AssetManager, SelectedActor) 프레임 컨텍스트 준비
SceneManager → World → Actor.SubmitRenderInfos
  PrimitiveComponent → MeshInfos / InstancedMeshInfos
  BillboardComponent / ParticleSubUVComponent → QuadInfos
  NameComponent → TextInfos
Gizmo.SubmitRenderInfos → GizmoInfos
RenderingPipeline.Render(Collector) → 각 전용 파이프라인
```

컴포넌트가 출력 종류를 선택한다. 관리자는 범용 정보를 받은 뒤 플래그로 분류하거나 파티클 정보를 쿼드로 변환하지 않는다. 파이프라인에는 그 종류의 출력에 필요한 필드만 전달한다.

## 데이터 타입

| 타입 | 필드/역할 | 사용하는 곳 |
|---|---|---|
| `FRenderMeshInfo` | 메시·텍스처 에셋, 월드 행렬, 색상, UV scale/offset | mesh, instanced mesh, stencil, gizmo |
| `FRenderQuadInfo` | Model, 텍스처·색상, 현재/다음 UV, 보간 비율, 합성·깊이·주소 모드 | quad |
| `FRenderTextInfo` | 글자 메시, 폰트 에셋, 위치·크기·색상 | text |
| `FRenderLineInfo` | 시작점·끝점·색상·두께 | line |
| `FRenderFullscreenInfo` | 메시·텍스처 에셋 | fullscreen |
| `FRenderWorldAxisInfo` / `FRenderWorldGridInfo` | 축 방향·색·굵기 / 그리드 간격 | world axis / world grid |
| `FPickInfo` | 프리미티브 종류, 오브젝트 ID, 실제 출력 행렬, 로컬·월드 바운드 | CPU 피킹 전용; Collector 밖에서 수집 |

`FRenderCollector`의 `MeshInfos`는 일반 메시 pass, `InstancedMeshInfos`는 정점색 프리미티브의 인스턴싱 pass, `GizmoInfos`는 깊이를 지운 뒤 solid 메시 pass로 전달된다. 모두 `FRenderMeshInfo`를 사용한다. `QuadInfos`의 불투명/투명/overlay 단계 선택은 쿼드 파이프라인이 깊이 설정으로 결정한다.

Collector의 `View`는 `FRenderView`다. 카메라 스냅샷, View/Projection/ViewProjection, 2D 투영, 뷰포트 크기, 프러스텀과 투영 보간 비율을 프레임당 한 번 준비한다. 컴포넌트와 모든 장면 파이프라인이 같은 값을 참조한다. 관리자의 중복 카메라·행렬 캐시는 없다.

Collector에는 AssetManager/ShowFlags/SelectedActor도 있다. AssetManager는 엔진 루프가 소유한 매니저의 비소유 포인터다. 컴포넌트는 `GetAssetAs<T>(Name, true)`로 에셋을 직접 조회하고, 바운드와 표시 조건을 확인한 뒤 알맞은 배열에 작성한다. `FRenderAssets`와 별도 메시·텍스처 보관 맵은 제거했다. `BeginFrame`이 컨텍스트와 월드 축·그리드 배열을 초기화한다. `Clear()`는 모든 제출 배열만 비우며 컨텍스트를 보존한다. 축·그리드도 비워지므로 다음 프레임은 다시 `BeginFrame`으로 시작한다. 등록·언로드 흐름은 [AssetManagerDirectAccess.md](AssetManagerDirectAccess.md)를 참고한다.

장면 pass의 호출 형태는 `Draw(전용 정보 배열, const FRenderView&)`이다. 쿼드는 추가로 실행 단계를 받고, 카메라를 사용하지 않는 fullscreen은 전용 배열만 받는다. 텍스트도 배열을 받아 내부에서 순회한다. 여기서 배치는 API 호출 단위이며 텍스트 전체를 GPU draw 한 번으로 합친다는 뜻은 아니다. 인스턴싱 pass는 입력을 변경하지 않아 배열을 const로 받고, 정렬·소비하는 pass는 nonconst로 받는다. `Render(Collector)`는 전용 배열을 정해진 순서로 전달한다.

## 파티클 보간의 책임

`UParticleSubUVComponent::Update`는 현재·다음 프레임과 경과 비율을 계산한다. `SubmitRenderInfos`는 그 값을 현재/다음 UV 사각형 및 `FrameBlend`로 작성하고 `Collector.QuadInfos`에 직접 넣는다. 전용 중간 파티클 렌더 정보나 관리자의 변환 단계는 없다.

`FrameBlend = 0`이면 현재 프레임만 표시한다. 별도 보간 ON/OFF 옵션은 없다. `BlendMode`는 배경과의 합성, `AddressMode`는 UV 범위 밖 샘플링이므로 프레임 보간 여부와 다르다.

빌보드 Model은 컴포넌트가 현재 카메라 축과 컴포넌트 크기로 만든다. 피킹도 동일한 `GetRenderTransform`을 사용한다. 선택/피킹 메타데이터에는 텍스처·폰트·파티클 재생 필드가 들어가지 않는다.

## 피킹·컬링·에디터

- 컴포넌트는 프러스텀과 표시 플래그를 제출 시 확인한다. 선택된 액터의 컴포넌트만 바운드를 제출하며, 컴포넌트의 바운드 표시 설정과 `SF_BoundingBox`가 켜져 있고 화면에 보일 때 `Collector.AddBounds`로 `LineInfos`에 12개 선분을 작성한다. 중간 `BoundsInfos`는 없다.
- 선택 가능한 컴포넌트만 별도 `SubmitPickInfos`로 CPU 피킹 정보를 작성한다. 이름표는 선택 대상에 넣지 않는다. 표시 플래그가 꺼져도 피킹 정보는 유지한다. Collector에 피킹 배열을 저장하지 않는다.
- 에디터 레이캐스트는 `SceneManager.GetPickInfos(Camera)`로 현재 컴포넌트의 변환·바운드를 수집한다. 이전 프레임의 범용 배열을 참조하지 않는다.
- 앱은 컴포넌트 업데이트 후 피킹을 처리하고, 에디터 조작이 끝난 현재 상태로 렌더 정보를 제출한다.
- 선택된 액터의 프리미티브 컴포넌트가 메시·실제 출력 행렬을 `SelectionInfos`에 직접 작성한다. 두 stencil pass가 같은 배열을 사용하며, 화면상 약 3px 테두리를 위한 확장은 outline pass 내부에서 계산한다. 선택된 액터에 여러 프리미티브가 있으면 각각 제출한다.
- 그리드·월드 축은 표시 설정에 따라 `FRenderWorldGridInfo`/`FRenderWorldAxisInfo` 배열을 작성하고 기존 전용 파이프라인의 `Draw`에 직접 전달한다. WEEK3 셰이더를 수정하지 않고 사용한다. 그리드가 X/Y축을 자체 표시하므로 함께 켰을 때 전용 축 pass는 Z축만 그린다. 그리드를 끄고 축만 켜면 축 pass가 X/Y/Z를 모두 그린다. 축 표시를 꺼도 그리드 자체의 X/Y 강조색은 남는다.
- 기존 CPU 축·그리드 선분 생성 함수와 고정 그리드 범위 `mgridExtent`는 제거했다. 길이·굵기·그리드 범위는 WEEK3 셰이더 표현을 따른다. 그리드 간격은 기존 에디터 설정값을 그대로 전달한다.
- 일반 선분은 `Collector.LineInfos` 한 배열만 사용한다. 바운드 선분도 이 배열에 직접 추가하고 `FLineGraphicsPipeline::Draw`에 전달한다. 관리자 내부의 `mLineInfos` 복사본, `DrawLine`/`DrawAABBLine`/`FlushLines`는 제거했다. 프레임 데이터의 수명은 Collector가 관리한다.

## 제거·교체한 항목

| 항목 | 이전 역할 | 현재 대체 |
|---|---|---|
| 전역 `FRenderInfo` | 메시·텍스트·파티클·피킹 정보를 모두 보관 | 종류별 타입과 `FPickInfo` |
| `ERenderFlags`, 비트 연산자, `HasAllRenderFlags`/`HasAnyRenderFlags` | 범용 정보의 출력 종류 판별 | 컴포넌트가 배열 선택 |
| `ERenderQueueType`, `RQT_*`, `updateRenderQueue`, `CopyInfos` | 범용 배열을 재분류·복사 | Collector의 전용 배열 |
| `renderParticle` 및 관리자 내 프레임→UV 변환 | 범용 파티클을 쿼드 정보로 변환 | 파티클 컴포넌트의 직접 제출 |
| `renderTexturedPrimitive`, `renderSimplePrimitiveInstanced`, `renderBillboardText`, `renderGizmo` 전달 래퍼 | 범용 큐를 개별 pass에 전달 | `Render`에서 전용 배열 직접 전달 |
| `renderBoundingBox` | 범용 정보에서 바운드 추출 | 컴포넌트가 `AddBounds`로 선분 직접 제출 |
| `Update(deltaTime, outRenderInfos)` | 상태 갱신과 렌더 정보 작성 결합 | `Update(deltaTime)` + `SubmitRenderInfos(Collector)` |
| `makeRenderInfo`, `GetRenderInfos`, `GetFirstRenderInfo` | 범용 정보 생성/조회 | 전용 제출 및 피킹 API |
| World의 `mRenderInfos`/예약 상수, SceneManager의 범용 조회 | 이전 업데이트의 범용 결과 보관 | 현재 상태에서 Collector에 제출 |
| `GetAxisRenderInfos` | 축 출력을 위한 플래그만 있는 항목 | 관리자에서 표시 설정 확인 |
| `GetGizmoRenderInfo` | 범용 기즈모 배열 반환 | `Gizmo.SubmitRenderInfos` |
| 파티클의 `mSubUVMesh`와 갱신 코드 | 실제 파티클 보간 출력에 쓰이지 않는 중복 UV 캐시 | 컴포넌트가 두 UV 영역 직접 계산 |

모든 pass의 클래스 내부 `FRenderInfo` 별칭도 제거했다. 선언에서 `FRenderMeshInfo`, `FRenderQuadInfo`처럼 실제로 받는 타입을 바로 확인할 수 있다.

구의 회전 텍스처는 누적 회전각으로 `FRenderMeshInfo.UVOffset`을 직접 작성한다. 중간 `FSubUVMesh` 객체와 파일은 제거했다. 기존 저장 데이터의 파티클 블렌드 모드·행/열·재생 설정은 유지했다. 이후 일관성 정리에서 삭제·교체한 전체 항목은 [RenderingConsistencyCleanup.md](RenderingConsistencyCleanup.md)에 정리했다.

## 짧은 검증

Debug/x64 빌드 후 `python Tests/run_renderer_merge.py`로 제한된 smoke 테스트를 실행한다. 기본 실행은 컴포넌트별 배열 선택, 구 UV, 파티클 보간값, 실제 GPU 제출, 텍스트 배치 내 잘못된 항목 건너뛰기, 컬링·표시 플래그, 선택 테두리, 빌보드와 피킹 변환 일치, 회전된 비균일 스케일, 쿼드 보간/깊이 정렬/합성을 확인한다.

기존 넓은 GPU 검증은 `python Tests/run_renderer_merge.py --full`로 명시적으로 선택한다. 이번 작업에서는 전체 단위 테스트와 `--full`은 반복하지 않는다. 실제 창의 수동 시각 검증 대신 WARP/D3D11 debug 장치의 드로우 수와 픽셀을 확인한다.

로그: `Saved/renderer-merge-build.log`, `Saved/render-consistency-quick.log`.
