# GraphicsManager → RenderingPipeline 전환

> 후속 수정: 범용 `FRenderInfo`와 관리자의 재분류·변환을 제거하고 컴포넌트가 종류별 Collector 배열에 직접 제출하도록 변경했다. **현재 제출 구조와 이번 검증 범위는 [TypedRenderCollector.md](TypedRenderCollector.md)를 기준으로 한다.** 아래 내용 중 범용 정보/관리자 변환에 관한 설명은 그 전 단계의 기록이다.

에셋 전환 커밋 `b867c5f` 이후의 관리자 전환 및 후속 쿼드/파티클 통합을 정리했다. 이전 에셋 전환 기록은 `AssetSystemMigration.md`에 보존한다. 그 문서의 `FGraphicsManager`/렌더러 직접 Draw 설명은 당시 상태이며, 현재 실행 경로는 이 문서와 같다. 표의 '신규'는 클래스가 새로 생겼다는 뜻이며, WEEK4에 없던 기능을 추가했다는 뜻은 아니다.

## 실행 경로와 책임

`FEngineLoop → FRenderingPipeline::Render → 각 FGraphicsPipeline::Draw → D3D11`

- 앱 초기화, 프레임 출력, 종료, 에디터 UI 참조, 표시 옵션 명령, 그리드 간격 명령, 화면 크기 변경을 `FRenderingPipeline`에 연결했다. `FGraphicsManager` 객체는 생성되지 않으며 클래스와 파일도 삭제했다.
- `FRenderingPipeline`: 에셋 등록, 투영 계산, 에셋 연결, 프러스텀 컬링, 표시 플래그, 렌더 큐 분류·순서, 파티클 정렬, 선택 테두리, 기즈모, 프레임 표시를 담당한다.
- 각 그래픽스 파이프라인: 해당 출력에 필요한 셰이더·상수 버퍼·입력 레이아웃·렌더 상태·드로우 호출을 소유한다.
- `URenderer`: D3D 장치/스왑 체인, 렌더 타깃과 깊이 버퍼, 뷰포트, 공통 버퍼·텍스처 생성, 상태 풀, 프레임 clear/present만 담당한다. 메시·폰트·파티클별 출력 함수는 없다.
- `FRenderAssets`와 `FAssetManager`: 앞선 작업의 공유 에셋 로딩·수명을 그대로 담당한다. 직접 DDS/PNG 파일 로딩 경로를 되살리지 않았다.

| 실제 출력 | 담당 파이프라인 | 동작 |
|---|---|---|
| 일반 프리미티브 | `FInstancedMeshGraphicsPipeline` (신규) | 메시별 그룹, 인스턴스 행렬·색상, 최대 1,024개씩 배치 출력 |
| 텍스처 프리미티브 | `FMeshGraphicsPipeline` | 에셋 바인딩, 오브젝트 색상 곱, SubUV scale/offset |
| 이름표 | `FTextGraphicsPipeline` (신규) | 동일한 폰트 에셋으로 bitmap/MSDF 선택, 글리프 배치와 거리값 사용 |
| 그리드·월드 축·경계 상자 | `FLineGraphicsPipeline` | 1픽셀 선분을 모아서 출력, 깊이 검사·쓰기 |
| 파티클 | `FQuadGraphicsPipeline` (확장) | CPU에서 빌보드 행렬 구성, 현재/다음 프레임 SubUV 보간, 블렌드 모드, 깊이 쓰기 비활성화 |
| 선택 테두리 | `FStencilMarkGraphicsPipeline` + `FStencilOutlineGraphicsPipeline` | 원본 스텐실 기록 후 확대 메시 출력 |
| 기즈모 | solid 전용 `FMeshGraphicsPipeline` 인스턴스 | 장면 깊이를 지운 뒤 출력; 와이어프레임 모드에서도 solid |
| 로딩 화면 | `FFullscreenGraphicsPipeline` (신규) | 기존 화면 쿼드·텍스처 에셋 사용; 깊이 검사 없음 |

장면 출력 순서는 일반 프리미티브 → 텍스처 프리미티브 → 이름표 → 월드 축/그리드/경계 상자 → 먼 파티클부터 → 선택 테두리 → 깊이 초기화 → 기즈모이다. 그 뒤 앱에서 ImGui를 그린다.

## 유지한 기능과 달라진 내부 구현

- 기존 앱의 백버퍼를 그대로 사용한다. 실제 표시되지 않던 별도의 scene render target 경로를 제거했다. ImGui가 사용할 백버퍼와 에디터 뷰포트의 좌표·크기를 유지한다.
- 기존 앱 기준 near/far는 `0.1 / 100`, 원근/직교 혼합 투영 및 1초 smoothstep 전환을 유지한다. 미연결 WEEK3 구현에 있던 far=2000을 앱에 새로 적용하지 않았다.
- 기존 show flags와 설정 명령 값을 유지한다. 와이어프레임은 뒷면 제거 없이 표시하고, 기즈모·폰트·파티클·테두리는 solid로 그린다.
- 기존 정점색과 tint 혼합, 텍스처 tint 곱셈, SubUV, 파티클 프레임 보간·정렬·블렌딩을 유지한다. 상태 풀의 투명/가산 alpha 누적도 이전 앱 방식과 맞췄다.
- 월드 축은 기존의 ±50 길이, 음수 축의 어두운 색을 유지한다. 그리드는 기존 유한 영역 1000과 실수 간격 설정을 유지한다. WEEK3의 무한 그리드/축 유틸리티를 대신 켜지 않았다.
- 기존 고정 크기 선분 VB/IB에 매 프레임 복사하던 방식은 `FRenderLineInfo` structured buffer 배치로 바꿨다. 카메라 평면을 가로지르는 선을 투영 전 잘라 나눗셈으로 인한 깨짐을 방지한다.
- 텍스트 정점도 `FVertexSimple`로 통일했다. 위치·UV·글리프 배치는 유지하고, 별도 `FVertexTextured` 입력 레이아웃을 없앴다.
- 스왑 체인은 기존과 같이 `Present(1, 0)`으로 표시한다.

## 쿼드의 프레임 보간과 파티클 통합

WEEK4 베이스 커밋 `2a02621`의 `ShaderParticle.hlsl`부터 두 프레임의 색상 보간은 존재했다. 관리자 전환 중 별도로 만들었던 파티클 pass를 제거하고 이 기능을 기존 쿼드 pass에 합쳤다.

데이터 흐름은 다음과 같다.

`UParticleSubUVComponent → FRenderInfo → FRenderingPipeline::renderParticle → FRenderQuadInfo → FQuadGraphicsPipeline → Quad.hlsl`

- 컴포넌트가 `currentFrame`, `nextFrame`, `frameRatio`를 작성한다. 렌더링 관리자는 프레임 번호를 UV 사각형으로 변환하고 `frameRatio`를 `FrameBlend`에 그대로 전달한다. 재생 시간이나 프레임 진행을 렌더러가 계산하지 않는다.
- `FRenderQuadInfo.SubUV`는 현재 영역, `NextSubUV`는 다음 영역이며 둘 다 `(offsetU, offsetV, sizeU, sizeV)` 형식이다.
- `FrameBlend` 기본값은 `0`이다. `0` 이하이면 현재 영역만 샘플링한다. 양수이면 현재/다음 색상을 `saturate(FrameBlend)`로 보간한다. 별도의 보간 사용 여부 옵션은 없다. 일반 쿼드는 기존 필드만 작성해도 동작한다.
- GPU에는 128바이트 `FQuadConstants`를 보낸다. C++과 HLSL 모두 Model/Color/SubUV/NextSubUV 뒤에 float/int/int/int를 배치해 패킹을 맞췄다.
- 쿼드의 `Model`에 빌보드 변환을 담는다. Y행은 CameraRight × Scale.y, Z행은 CameraUp × Scale.z, 이동행은 파티클 위치로 채운다. 기존 YZ 평면 파티클의 위치·크기 계산과 같다.
- 기존 `BlendMode`는 프레임 보간과 별개로 배경과의 합성(불투명/알파/가산/색상 쓰기 금지)을 지정한다. 컴포넌트의 기존 `EBlendStateType`은 유지하며 관리자가 대응하는 값으로 변환한다.
- 새 `AddressMode`는 UV 범위 밖 샘플링 방식이다. 기본값 Wrap은 일반 쿼드의 기존 동작이고, 파티클 제출 시 Clamp를 지정해 이전 파티클 동작을 유지한다. 보간 ON/OFF 설정이 아니다. 샘플러는 공통 상태 풀에서 재사용한다.
- 파티클은 깊이 검사 활성/쓰기 비활성으로 Transparent 단계에 제출한다. 기존 먼 순서 정렬을 유지하며, 쿼드는 투명/overlay 입력 순서대로 출력하고 나머지 항목을 순서대로 압축한다. 불투명 분리도 `stable_partition`으로 변경해 다른 단계의 순서를 보존한다.
- 쿼드는 기존 `DrawProcedural(6)` 경로를 사용한다. 별도 파티클 메시 업로드가 필요 없어 `Mesh.Particle` 등록, `ParticleMesh` 멤버/해제, `GetParticleMesh()` 및 이제 사용하지 않는 `QuadTextureIndexedVertices/Indices`를 제거했다. 일반 프리미티브의 `Mesh.Quad`와 미리 정의된 `FVertexSimple` 정점·인덱스는 유지한다.

후속 통합에서 제거한 파일과 역할:

| 파일 | 통합 전 역할 | 대체 |
|---|---|---|
| `Rendering/pipelines/FParticleGraphicsPipeline.h` | 파티클 전용 pass API | 기존 `FQuadGraphicsPipeline` |
| `Rendering/pipelines/FParticleGraphicsPipeline.cpp` | 파티클 상수·셰이더·상태·메시 출력 | 관리자의 쿼드 정보 구성 + 쿼드 pass |
| `Assets/Shaders/Particle.hlsl` | GPU 빌보드 변환 및 두 프레임 색상 보간 | CPU Model 행렬 + `Quad.hlsl` |

위 경로는 `EngineLib/` 기준이다. 세 파일의 프로젝트·필터 등록도 제거했다. 전용 `FParticleConstants`, `mParticlePipeline` 및 생성/해제 코드도 없어졌으며 관리자는 `mQuadPipeline`을 소유한다.

## 관리자 전환에서 삭제한 파일 전체

| 파일 | 이전 역할 | 대체 |
|---|---|---|
| `EngineLib/Rendering/GraphicsManager.h` | 기존 렌더링 관리자 API·설정·리소스 참조 선언 | `RenderingPipeline.h` |
| `EngineLib/Rendering/GraphicsManager.cpp` | 렌더 큐·개별 렌더러 호출·에셋 등록·프레임 출력 | `RenderingPipeline.cpp` 및 개별 pass |
| `EngineLib/Shaders/ShaderW0.hlsl` | 단순 메시·인스턴스의 변환과 tint 처리 | `Assets/Shaders/Mesh.hlsl`, `InstancedMesh.hlsl` |
| `EngineLib/Shaders/ShaderTexture.hlsl` | 텍스처 메시 및 SubUV 출력 | `Assets/Shaders/Mesh.hlsl` |
| `EngineLib/Shaders/ShaderLine.hlsl` | CPU 정점/인덱스 선분 출력 | `Assets/Shaders/Line.hlsl` |
| `EngineLib/Shaders/ShaderFont.hlsl` | bitmap 폰트 빌보드와 coverage | `Assets/Shaders/Text.hlsl` |
| `EngineLib/Shaders/ShaderFontMSDF.hlsl` | MSDF 폰트 빌보드와 거리 기반 coverage | `Assets/Shaders/Text.hlsl`에 통합 |
| `EngineLib/Shaders/ShaderParticle.hlsl` | 파티클 빌보드·두 SubUV 프레임 보간 | CPU Model 구성 + `Assets/Shaders/Quad.hlsl` |
| `EngineLib/Shaders/ShaderLoadingScreen.hlsl` | 전체 화면 텍스처 표시 | `Assets/Shaders/Fullscreen.hlsl`로 이동 |

위 파일의 VS 프로젝트 항목·필터 항목 및 빈 `Shaders` 필터도 제거했다. 최종적으로 남은 신규 pass 3종의 헤더/소스 6개와 shader 3개는 각각 `Rendering/pipelines`, `Assets/Shaders` 필터에 등록했다.

## URenderer에서 제거한 함수 전체

기능 자체가 없어진 것이 아니라 대부분 해당 pass로 옮겨졌다. 렌더러의 일반적인 장치/타깃/자원 API는 유지한다.

| 제거한 함수 | 이전 역할 | 대체 |
|---|---|---|
| `InitializeDeviceResources` | 모든 종류의 셰이더·상태·동적 버퍼 일괄 초기화 | 각 pass 생성자 |
| `createShader`, `releaseShader` | 모든 VS/PS와 입력 레이아웃 생성·수동 해제 | 각 pass의 `SetShader`, `ComPtr` 수명 |
| `createFullscreenShader` | 로딩 화면 셰이더/레이아웃 생성 | `FFullscreenGraphicsPipeline` |
| `createConstantBuffer`, `releaseConstantBuffer` | 종류별 전역 상수 버퍼 배열 관리 | 각 pass의 `AddConstantBuffer` |
| `createRasterizerState`, `releaseRasterizerState` | 전역 solid/wireframe 상태 배열 관리 | `FGraphicsPipeline::SetRasterizerState` |
| `createDepthStencilState`, `releaseDepthStencilState` | 전역 깊이·스텐실 상태 배열 관리 | 공유 `FDepthStencilStatePool` 및 pass별 선택 |
| `createBlendState`, `releaseBlendState` | 전역 블렌드 상태 배열 관리 | 공유 `FBlendStatePool` 및 pass별 선택 |
| `PrepareForUI` | 타깃 clear 및 바인딩 | 프레임 `Prepare` + fullscreen pass |
| `PrepareTexturedPrimitive`, `prepareTextureShader` | 텍스처 메시용 상태·셰이더 설정 | `FMeshGraphicsPipeline` |
| `PrepareSimpleInstanced`, `prepareInstancedShader` | 인스턴스 메시용 상태·셰이더 설정 | `FInstancedMeshGraphicsPipeline` |
| `PrepareGizmo`, `prepareSimpleShader` | solid 메시/기즈모용 상태 설정 | solid 전용 mesh pass |
| `PrepareLine`, `prepareLineShader` | line-list 상태·셰이더 설정 | `FLineGraphicsPipeline` |
| `PrepareParticle`, `prepareParticleShader` | 파티클용 상태·셰이더 설정 | `FQuadGraphicsPipeline` |
| `PrepareHighlight` | 선택 테두리용 상태 준비 | 두 stencil pass |
| `UpdateSimpleConstant` | 메시 변환·카메라·tint 업로드 | mesh/instanced/stencil pass |
| `UpdateTextureConstant` | 텍스처 메시 변환·tint·SubUV 업로드 | mesh pass |
| `UpdateFontConstant` | 이름표 위치·크기·카메라 축·tint 업로드 | text pass |
| `UpdateParticleConstant` | 파티클 위치·카메라·프레임 보간 업로드 | 관리자의 쿼드 정보 구성 + quad pass |
| `UpdateBlendState` | 전역 배열에서 블렌드 상태 선택 | quad pass의 `SetBlendState` |
| `RenderSimplePrimitive` | 정점/인덱스 버퍼를 직접 그리는 종류별 API | mesh/stencil pass |
| `RenderSimpleInstanced` | 두 VB 슬롯으로 인스턴싱 | structured buffer를 사용하는 instanced pass |
| `EnsureInstanceCapacity` | 전역 인스턴스 VB 재할당 | 1,024개 단위 structured buffer 배치 |
| `RenderTexturedMesh` | 메시·텍스처·샘플러 직접 바인딩/출력 | mesh/quad/fullscreen pass |
| `RenderText` | 렌더러 내 bitmap/MSDF 선택 및 동적 글자 출력 | text pass |
| `RenderFullscreenTexture` | 로딩 화면 출력 | fullscreen pass |
| `RenderHighlight` | 스텐실 기록·확대 메시 출력 | stencil mark/outline pass |
| `RenderLines` | 고정 line VB/IB 갱신 및 출력 | line pass |
| `createLineVertexBuffer`, `releaseLineVertexBuffer` | 고정 크기 선분 VB 생성·해제 | line pass의 structured buffer |
| `createLineIndexBuffer`, `releaseLineIndexBuffer` | 고정 크기 선분 IB 생성·해제 | 선분 pass에서 출력; 별도 CPU 선분 인덱스 없음 |

`UploadTextBuffer` 내부 헬퍼는 삭제한 기능이 아니다. 텍스트 pass 소스로 이동했다.

## 제거한 타입·멤버·상수

| 항목 | 이전 역할 | 현재 |
|---|---|---|
| `FConstants`, `FTextureConstants`, `FParticleConstants`, `FFontConstants`, `FUnicodeFontConstants` (렌더러 전역) | 종류별 상수 버퍼의 공용 자료형 | 필요한 pass의 구현 파일 내부 자료형 |
| `FInstanceData` | 렌더러 외부에 노출된 인스턴스 VB 자료형 | instanced pass 내부의 `FMeshInstance` |
| `FVertexTextured` | 글자 전용 position/UV 정점 구조체 | `FVertexSimple` |
| `LINE_VERTEX_CAPACITY`, `LINE_INDEX_CAPACITY` | 고정 선분 버퍼 크기 | line pass가 배치 단위로 출력 |
| `EContantBufferType` 및 `CBT_*` | 렌더러 상수 버퍼 배열 인덱스 | pass별 상수 슬롯 |
| `EVertexShaderType`, `EPixelShaderType` 및 `VST_*`, `PST_*` | 렌더러 셰이더 배열 인덱스 | pass별 셰이더 객체 |
| `EDepthStencilStateType`, `DSS_*` | 전역 깊이/스텐실 배열 인덱스 | 상태 풀의 명시적 키 |
| `RasterizerState`, `ConstantBuffer`, `DepthStencilState`, `BlendState`, `VertexShader`, `PixelShader` 배열 | 렌더러가 모든 종류의 상태를 동시에 소유 | pass 객체와 공통 상태 풀 |
| `SimpleInputLayout`, `LineSimpleInputLayout`, `PrimitiveTextureLayout`, `FontInputLayout`, `InstancedInputLayout` | 종류별 별도 입력 레이아웃 | `FGraphicsPipeline`의 표준 입력 레이아웃 |
| `LineVertexBuffer`, `LineVertexCapacity`, `LineIndexBuffer`, `LineIndexCapacity` | 렌더러 내부 선분 자원/용량 | line pass |
| `InstanceBuffer`, `InstanceCapacity` | 렌더러 내부 인스턴스 자원/용량 | instanced pass |
| `TextVertexBuffer`, `TextIndexBuffer`, `MSDFConstantBuffer` | 렌더러 내부 글자 자원 | text pass |
| `LoadingScreenVertexShader`, `LoadingScreenPixelShader`, `LoadingScreenInputLayout` | 렌더러 내부 로딩 화면 셰이더 | fullscreen pass |
| `StrideSimple`, 렌더러 `mbWireFrame` | 공통 정점 stride/별도 와이어프레임 상태 | 표준 레이아웃과 `EViewModeIndex` |

`EBlendStateType`은 파티클 컴포넌트·에디터·저장 데이터에 실제로 사용하므로 유지했다. 관리자가 쿼드 정보 구성 시 `ERenderBlendMode`로 변환한다.

## GraphicsManager API·로직의 이동과 제거

클래스와 두 파일은 삭제했지만 아래 동작은 `FRenderingPipeline`으로 이동했다.

| 이전 함수/설정 | 역할과 현재 위치 |
|---|---|
| 생성자·소멸자, `GetRenderer` | 장치/에셋/pass 수명; 새 pipeline이 담당 |
| `InitializeAssets`, `InitializeLoadingScreen`, `RenderLoadingScreen` | 기본 에셋 및 로딩 화면; 새 pipeline과 fullscreen pass |
| `Prepare`, `Render`, `updateRenderQueue`, `Display` | 프레임·투영·큐 분류·출력; 새 pipeline |
| `renderSimplePrimitiveInstanced`, `renderTexturedPrimitive` | 메시별 출력; 새 pipeline이 개별 pass에 제출 |
| `renderBillboardText`, `renderParticle` | 폰트/파티클 출력; 새 pipeline이 text/quad pass에 제출 |
| `renderGizmo`, `renderHighLight` | 기즈모/선택 강조; mesh/stencil pass에 제출 |
| `DrawLine`, `DrawAABBLine`, `FlushLines`, `renderGrid`, `renderWorldAxis`, `renderBoundingBox` | 선분 수집; `FRenderLineInfo`로 통일하여 line pass에 제출 |
| `GetGridWidth`, `SetGridWidth`, 그리드 extent/spacing | 기존 유한 그리드 설정 유지 |
| `HasShowFlag`, `SetShowFlag`, `GetShowFlags`, `SetShowFlags` | 에디터 표시 플래그 유지 |
| `GetPerspectiveRatio`, `SetPerspectiveRatio`, `StartProjectionTransition`, `IsOrthographicTarget`, `UpdateProjectionTransition` | 혼합 투영 및 전환 유지 |
| `GetAspect` | 현재 에디터 뷰포트 비율 |
| `SetViewMode`, `GetViewMode` | `SetViewModeIndex`, `GetViewModeIndex`로 이름 통일 |

다음은 대체 또는 중복/미사용으로 제거했다.

- `CalculateLineBuffer`: 매 프레임 선분 VB/IB 예상 용량 계산. CPU 정점·인덱스 방식이 없어져 삭제했다.
- `mLineVertices`, `mLineIndices`: CPU 선분 정점·인덱스 보관. `mLineInfos`로 대체했다.
- `Update`: 뷰포트 비율만 갱신하던 함수. 새 `Prepare`에서 계산한다.
- `GetWireFrame`, `SetWireFrame`, `mbWireFrame`: view mode와 중복되는 설정. `EViewModeIndex` 하나로 통일했다.
- `IsPerspectiveProjection`, `SetPerspectiveProjection`, `mbPerspectiveProjection`: 실제 혼합 투영 계산과 분리된 미사용 bool. 실제 projection ratio/transition을 유지한다.
- `GetShowWorldAxis`, `SetShowWorldAxis`, `mbShowWorldAxis`: 실제 `mShowFlags`와 분리된 미사용 표시 bool. 실제 show flags를 유지한다.
- `GetCameraOrthoDistance`, `SetCameraOrthoDistance`: 외부 호출 없는 캐시 getter/setter. 현재 카메라 값은 `Prepare`에서 계속 읽는다.
- 컴포넌트/SceneManager/에디터의 옛 manager 선언, 불필요한 include, 주석 처리된 manager 직접 호출 예시도 제거했다.

## 기존 미연결 RenderingPipeline 구현에서 정리한 부분

- `Prepare(Camera, width, height)`는 실제 앱 뷰포트를 사용하는 `Prepare(Camera)`로 교체했다.
- 인자 없는 `Render()`는 실제 장면/기즈모/축/카메라/선택 액터를 받는 `Render(...)`로 교체했다.
- 사용하지 않던 `mSceneRenderTarget`, `mSceneDepthStencil`, `GetSceneRenderTarget`와 별도 scene target 생성·resize 경로를 제거했다. `OnResize`는 실제 백버퍼 resize로 전달한다.
- 실제 호출부가 없던 `GetRenderCollector`, `GetRenderInfos`, pipeline 내부 `mRenderCollector`를 제거했다. `FRenderCollector` 자료형 자체는 컴포넌트 제출/유틸리티 용도로 유지한다.
- 계산만 하고 사용하지 않던 `mViewMatrix`, `mProjectionMatrix`, `mViewProjectionMatrix`, `mViewOrthogonalProjectionMatrix`를 제거하고 실제 혼합 투영 행렬을 유지한다.
- 빈 `Update`, 빈 `FlushLines` 구현을 제거했다. `FlushLines`는 이제 실제 line pass에 제출한다.
- 외부 수동 호출 형태의 `RenderHighLight`는 프레임의 선택 액터를 처리하는 내부 `renderHighLight`로 통합했다.
- 정수 그리드의 `GridGap`, `GetGridGap`, `SetGridGap` 및 단계별 반올림은 삭제했다. 기존 앱의 실수 `GridWidth` 설정을 사용한다.
- 실제 프레임에서 사용하지 않던 2D/world-axis/world-grid pass 소유 멤버와 `GetLine2DPipeline`, `GetCircle2DPipeline`, `GetTriangle2DPipeline` getter를 제거했다. 개별 pass 파일은 독립 유틸리티와 테스트에서 계속 사용한다. quad pass는 후속 통합에서 실제 파티클 출력에 연결했다.

## 남겨둔 코드의 이유

- 기존 WEEK3의 10개 개별 그래픽스 파이프라인은 유지하며, 최종 신규 pass는 3개이다. quad는 파티클에도 사용하며 2D/world-axis/world-grid는 일반 유틸리티로 남겨 둔다. 이 파일들을 구형 manager의 중복 구현으로 보지 않는다.
- ImGui의 UI 렌더러/TTF 로딩은 별도 백엔드이므로 유지했다.
- `FTextMesh`의 동적 글리프 배치, `FSubUVMesh`의 UV 설정, 프리미티브의 미리 정의된 정점·인덱스는 유지했다.
- 타깃 생성, structured buffer, 뷰포트, 상태 풀 등의 일반 `URenderer` API는 모든 pass가 사용하는 공통 기능이다.

## 검증

- `JungleEngine.sln` Debug/x64 전체 빌드 성공.
- WARP/D3D11 debug GPU 테스트 성공: 기존 10개 + 신규 3개 pass, 실제 드로우 수 및 색상 readback, 디버그 오류 없음.
- 실제 `FRenderingPipeline::Render` 검증: 에디터 뷰포트 위치·크기, 프러스텀 컬링, 표시 플래그, 텍스처/일반 메시, wireframe 및 cull-none, 투영 전환, 경계 상자, 축, 유한 그리드, 기즈모, 선택 테두리, 이름표, 파티클.
- 1,025개 인스턴스의 배치 경계 및 전체 정점 처리량 확인.
- 실제 픽셀 검증: 로딩 화면 텍스처, 메시의 tint/SubUV 빨강·파랑 샘플, 순서를 뒤집어 제출한 겹친 파티클의 정렬 및 alpha blend 결과.
- SubUV 픽셀 테스트로 HLSL scalar-array padding과 C++ 메모리 배치 차이를 발견했고, `int2 padding`으로 맞춘 후 통과했다.
- bitmap/MSDF 전환, 긴 문자열→짧은 문자열, 빈 문자열, 잘못된 인덱스, 잘못된 atlas 종류, 에셋 캐시/참조 수명 검증도 유지한다.
- 관리자 전환 단계에서 실행한 전체 단위 테스트 42개: 34개 통과, 기존과 동일한 8개 실패. 이전 로그와 실패 목록을 비교해 신규 실패가 없음을 확인했다. 후속 쿼드 통합은 전체 빌드와 아래 GPU 테스트로 검증했다.
- 쿼드 보간 비율 0/0.5/1의 빨강·보라·파랑 픽셀, Clamp→Wrap 전환, 텍스처 없는 쿼드, 혼합 렌더 단계의 순서 보존, 세 겹의 alpha/additive 합성, 깊이 쓰기 비활성 상태 검증.
- 실제 파티클 제출 경로에서 2×2 아틀라스의 서로 다른 행·열 프레임 보간, 카메라 90도 회전, 가로/세로가 다른 스케일, 잘못된 행 수의 출력 생략 검증.
- 프로젝트/필터 등록, 삭제 파일의 참조 부재, `git diff --check` 확인.
- 실제 앱 창을 띄워 수동으로 확인한 것은 아니다. 렌더링 검증은 화면 없는 WARP 장치에서 수행했다.

검증 코드: `Tests/RendererMergeSmoke.cpp`, 실행: `Tests/run_renderer_merge.py`.
검증 로그: `Saved/renderer-merge-build.log`, `Saved/quad-interpolation-gpu.log`, 이전 관리자 전환 단위 테스트 `Saved/pipeline-migration-unit-tests.log`.

기존 단위 테스트 실패 목록:

1. `TestActor.DeserializeClass_WhenFunctionCalled_InCorrectJson`
2. `TestFFileManager.ReadFileToString_WhenReadingFile_ReturnsCorrectContent`
3. `TestFFileManager.WriteStringToFile_WhenWritingOutsideRoot_ThrowsRuntimeError`
4. `TestFFileManager.IsUnder_WhenGivenPathIsNotUnderRoot_ReturnsFalse`
5. `TestUObject.DeseralizeClass_WhenGivenJson_DeserializeCorrectParams`
6. `TestFObjectFactory.LoadObject_WhenLoad_ReturnsCorrectObject`
7. `TestFObjectFactory.LoadObjectT_WhenLoad_ReturnsCorrectObject`
8. `TestUWorld.DeserializeClass_WhenFunctionCalled_InCorrectJson`
