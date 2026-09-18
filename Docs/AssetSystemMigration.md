# 에셋 시스템 전환 및 삭제 목록

이번 작업 시작 시점의 HEAD와 비교한 목록이다. 단순 줄 이동, 공백 정리, 제거한 설명 주석은 개별 기능 삭제로 세지 않았다. 주석으로 보관했던 전체 구형 구현은 별도 항목으로 명시했다.

## 현재 로딩·렌더링 경로

`FEngineLoop → FGraphicsManager::InitializeLoadingScreen/InitializeAssets → FRenderAssets → FAssetManager::Load → 해당 FAssetLoader → UAsset → FRenderInfo → Renderer/GraphicsPipeline`

- 파일 경로는 `FFileManager`의 루트 아래 상대 경로다. 기본 루트는 `Assets`이며, 현재 앱 작업 디렉터리는 `EngineLib`이다.
- 텍스처: `FFileAssetSource → FTexture2DAssetLoader → UTexture2DAsset`. DDS 압축 포맷과 제작된 mipmap을 보존하며, PNG/MSDF는 기존과 같은 선형 데이터로 읽는다.
- 폰트: `FFontAtlasAssetSource → FFontAtlasAssetLoader → UFontAtlasAsset`. 이미지와 글리프 메타데이터를 함께 보유한다. 이름표는 이 에셋을 공유 참조한다.
- 메시: `FStaticMeshAssetSource → FStaticMeshAssetLoader → UStaticMeshAsset`. `FVertexSimple` 정점과 미리 정의한 인덱스 배열을 업로드한다. 엔진에서 프리미티브 토폴로지를 재생성하지 않는다.
- `FAssetManager`는 이름별로 에셋을 재사용한다. 캐시를 비워도 컴포넌트/프레임 제출 정보가 참조 중인 에셋은 살아 있고, 마지막 공유 참조가 해제될 때 GPU 자원이 해제된다.
- 렌더러는 파일 경로를 받지 않는다. 메시/텍스처/폰트 에셋을 받아 GPU에 바인딩한다. 샘플러는 `FSamplerStatePool`을 공유한다.

| 용도 | 에셋 이름 | 소스 (`Assets` 기준) |
|---|---|---|
| 큐브 텍스처 | `Texture.Cube` | `Textures/CubeTextureSample.dds` |
| 구 텍스처 | `Texture.Earth` | `Textures/EarthTexture.dds` |
| 파티클 텍스처 | `Texture.Explosion` | `Textures/Explosion_Alpha.dds` |
| 로딩 화면 | `Texture.LoadingScreen` | `Textures/LoadingScreen.dds` |
| 기본 이름표 글꼴 | `Font.Korean` | `Fonts/KoreanFullAtlas.png` + `Fonts/KoreanFullAtlas.json` |
| 기본 메시 | `Mesh.Cube`, `Mesh.Sphere`, `Mesh.GizmoArrow`, `Mesh.Circle`, `Mesh.Triangle`, `Mesh.Quad` | 각 `Rendering/Primitives/*.h`의 정점·인덱스 배열 |
| 텍스처용 메시 | `Mesh.Cube.Textured`, `Mesh.Sphere.Textured` | `TexturedPrimitives.h`의 배열; 일반 쿼드는 `Mesh.Quad` 공유 |
| 파티클/화면 쿼드 | `Mesh.Particle`, `Mesh.Fullscreen` | `QuadTextureIndexed*`, `Fullscreen_*` 배열 |

## 삭제한 파일

| 파일 | 이전 역할 | 대체 |
|---|---|---|
| `EngineLib/ThirdParty/DirectX/WICTextureLoader.h` | 자체 WIC 이미지 로더 선언 | 에셋 로더가 vcpkg DirectXTK의 메모리 디코딩 API 사용 |
| `EngineLib/ThirdParty/DirectX/WICTextureLoader.cpp` | 파일/메모리 이미지 디코딩과 D3D 텍스처 생성 구현 | 위와 동일. 호출부가 없어 소스·프로젝트 항목·빈 VS 필터까지 제거 |

## 삭제 또는 통합한 함수 — URenderer

이 표의 함수는 모두 `EngineLib/Rendering/Renderer.h/.cpp`에서 제거했다. 새 함수로 기능을 옮긴 경우 대체 칸에 구분했다.

| 제거한 이름 | 이전 역할 | 대체/제거 이유 |
|---|---|---|
| `LoadTexture` | DDS 파일을 직접 읽어 raw SRV 반환 | `FTexture2DAssetLoader` |
| `createFontAtlasTexture` | 영어 DDS 폰트 아틀라스를 시작 시 항상 로딩 | 필요한 `UFontAtlasAsset`만 로딩 |
| `InitializeUnicodeFont` | 한글 PNG 직접 로딩 및 거리값 상수 버퍼 생성 | `FFontAtlasAssetLoader` + `RenderText`의 에셋별 거리값 |
| `CreateSamplerState` | 프리미티브마다 개별 샘플러 생성 | `FSamplerStatePool` |
| `createFontSamplerState` | 폰트 전용 샘플러 생성 | 상태 풀의 clamp 샘플러 |
| `createParticleStates` | 파티클 전용 샘플러 생성 | 상태 풀의 clamp 샘플러 |
| `ReleasePrimitiveTextureResources` | raw SRV와 샘플러 수동 해제 | 에셋의 `ComPtr` 수명 및 상태 풀 해제 |
| `ReleaseVertexBuffer` | 전달받은 raw 정점 버퍼 수동 해제 | `UStaticMeshAsset` 소유권으로 대체; 호출 없음 |
| `releaseFontAtlasTexture` | 영어 아틀라스 SRV 해제 | `UFontAtlasAsset` 수명으로 대체 |
| `releaseUnicodeFontAtlasTexture` | 한글 아틀라스 SRV 해제 | `UFontAtlasAsset` 수명으로 대체 |
| `releaseFontTexture` | 폰트 샘플러 해제 | 상태 풀로 대체 |
| `releaseFontBuffers` | ASCII 전용 정점/인덱스 버퍼 해제 | 공통 텍스트 `ComPtr` 버퍼로 대체 |
| `releaseUnicodeFontBuffers` | MSDF 전용 정점/인덱스/상수 버퍼 해제 | 공통 텍스트 버퍼 및 MSDF 상수 버퍼로 대체 |
| `PrepareFont` | ASCII 글꼴 그리기 상태 설정 | `RenderText`에서 에셋 종류로 선택 |
| `PrepareUnicodeFont` | MSDF 글꼴 그리기 상태 설정 | 위와 동일 |
| `prepareFontShader` | ASCII 셰이더·상수 바인딩 | 위와 동일 |
| `prepareUnicodeFontShader` | MSDF 셰이더·거리값 상수 바인딩 | 위와 동일 |
| `UpdateFontBuffer` | ASCII 텍스트 정점 갱신 | 공통 `UploadTextBuffer` |
| `UpdateUnicodeFontBuffer` | MSDF 텍스트 정점 갱신 | 공통 `UploadTextBuffer` |
| `ensureFontIndexBuffer` | 글자 수로 ASCII 인덱스 다시 생성 | `FTextMesh::Indices`를 그대로 업로드 |
| `ensureUnicodeFontIndexBuffer` | 쿼드 수로 MSDF 인덱스 다시 생성 | 위와 동일 |
| `RenderFontTexture` | 전역 ASCII 아틀라스로 텍스트 출력 | `RenderText(Mesh, Atlas)` |
| `RenderUnicodeFontTexture` | 전역 MSDF 아틀라스로 텍스트 출력 | 위와 동일 |
| `RenderTexturePrimitive` | raw 버퍼·SRV·샘플러·개수를 받아 출력 | `RenderTexturedMesh(Mesh, Texture, AddressMode)` |
| `RenderParticle` | 렌더러 소유 파티클 쿼드로 출력 | 파티클 메시 에셋 + 공통 `RenderTexturedMesh` |
| `createParticleVertexBuffer` | 렌더러 내부 파티클 정점 버퍼 생성 | `Mesh.Particle` 에셋 로딩 |
| `createParticleIndexBuffer` | 렌더러 내부 파티클 인덱스 버퍼 생성 | 위와 동일 |
| `CreateLoadingScreenResources` | 로딩 화면 메시·셰이더·샘플러 일괄 생성 | 메시/텍스처는 에셋; 셰이더만 `createFullscreenShader` 유지 |
| `PrepareSimplePrimitive` | 개별 단순 프리미티브 출력 준비 | 일반 프리미티브는 인스턴싱 경로를 사용하여 호출 없음 |
| `UpdateBillboardConstant` | 구형 텍스처 빌보드 상수 갱신 | 호출 없음; 폰트와 파티클은 각자의 실제 사용 경로 유지 |
| `prepareBillboardTextureShader` | 구형 텍스처 빌보드 셰이더 설정 | 호출 없음 |

`RenderSimplePrimitive`, `RenderSimpleInstanced`, `RenderHighlight`, `RenderFullscreenTexture`는 삭제한 기능이 아니다. raw 리소스/개수 인자를 `UStaticMeshAsset`/`UTexture2DAsset` 인자로 교체하고 모든 호출부를 수정했다.

## 삭제 또는 통합한 함수 — 그 외

| 제거한 이름 | 이전 역할 | 대체/제거 이유 |
|---|---|---|
| `FGraphicsManager::CreateBuffer` | 기본 프리미티브 버퍼 직접 생성·보관·경계 계산 | 메시 에셋 로딩 |
| `FGraphicsManager::CreateTexturedBuffer` | 텍스처 프리미티브 버퍼 직접 생성·보관 | 메시 에셋 로딩 |
| `FGraphicsManager::CreatePrimitiveTexture` | 파일 경로로 SRV/샘플러 직접 생성 | 텍스처 에셋 로딩 |
| `FGraphicsManager::renderSimplePrimitive` | 인스턴싱 전의 개별 프리미티브 출력 루프 | 실제 렌더 루프가 사용하지 않음 |
| `FGraphicsManager::RenderInstancingTest` | 테스트용 큐브 1만 개를 생성·출력 | 실제 호출 없음; 독립 GPU 테스트로 검증 |
| `FGraphicsManager::PrepareForUI` | 렌더러의 동명 함수를 그대로 전달 | 호출 없음; 로딩 화면에서 렌더러 직접 호출 |
| `FGraphicsManager::GetPrimitiveCenter` | 프리미티브 종류별 중심 하드코딩 | 메시 에셋의 로컬 경계값에서 계산 |
| `FGraphicsManager::GetPrimitiveHalfExtent` | 프리미티브 종류별 반크기 하드코딩 | 위와 동일 |
| `FRenderingPipeline::GetPrimitiveCenter` | 새 파이프라인에 남아 있던 동일 하드코딩 | 메시 에셋의 로컬 경계값에서 계산 |
| `FRenderingPipeline::GetPrimitiveHalfExtent` | 새 파이프라인에 남아 있던 동일 하드코딩 | 위와 동일 |
| `FObjectFactory::Initialize(FFontResource)` | 전역 기본 폰트 메타데이터의 raw 포인터 등록 | `SetDefaultFontAsset` 공유 참조 |
| `FObjectFactory::GetDefaultFontResource` | 위 raw 포인터 제공 | `GetDefaultFontAsset` |
| `UNameComponent::SetUnicodeNameText` | 한글 이름표만을 위한 별도 setter | `SetNameText`로 통합; 폰트 에셋이 MSDF/bitmap 방식을 결정 |
| `FFontResource::LoadUnicodeAtlas(path)` | JSON 파일을 직접 읽는 래퍼 | 에셋 소스가 파일 읽기; `LoadUnicodeAtlasFromString` 파서는 유지 |
| `FAssetLoader::UnloadAsset` | 로더의 해제 인터페이스 | 기존 구현들이 모두 no-op이었음; 공유 참조 수명으로 소유권 통일 |
| `FTexture2DAssetLoader::UnloadAsset` | 텍스처 로더의 no-op 해제 구현 | 위와 동일 |
| `FFontAtlasAssetLoader::UnloadAsset` | 폰트 로더의 no-op 해제 구현 | 위와 동일 |
| `FStaticMeshAssetLoader::UnloadAsset` | 메시 로더의 미구현 해제 stub | 위와 동일 |

`FStaticMeshAssetLoader::LoadAsset`에 있던 “파일 내용을 읽고, null 정점/인덱스로 빈 에셋을 생성”하던 TODO 구현도 제거했다. 함수 자체는 유지하며, 명시적인 정점·인덱스 소스를 받아 유효한 GPU 메시를 로딩한다.

## 삭제한 구조체·멤버·셰이더 항목

| 제거 항목 | 이전 역할 | 대체 |
|---|---|---|
| `FBuffer`의 `Buffer`, `SourceNum`, `LocalBounds`, `TexturedBuffer`, `IndexBuffer`, `IndexCount` | raw 메시 버퍼·개수·경계 저장; `TexturedBuffer`는 미사용 | `UStaticMeshAsset` |
| `FTexture`의 `SRV`, `Sampler` | raw 텍스처 뷰와 개별 샘플러 저장 | `UTexture2DAsset` + 상태 풀 |
| `mBufferMap`, `mTexturedBufferMap`, `mPrimitiveTextureMap` | `FGraphicsManager`의 raw 리소스 테이블 | `FRenderAssets`의 공유 에셋 목록 |
| `mLoadingScreenSRV` | 매니저가 별도 소유한 로딩 화면 SRV | `Texture.LoadingScreen` |
| `mbShowPrimitives` | 사용하지 않는 매니저 표시 플래그 | 실제 표시 설정은 `mShowFlags` |
| `FontAtlasShaderResoruceView`, `UnicodeFontAtlasSRV` | 렌더러 전역 ASCII/MSDF 아틀라스 | 제출된 `UFontAtlasAsset` |
| `FontTextureBuffer`, `FontIndexBuffer`, `UnicodeFontVertexBuffer`, `UnicodeFontIndexBuffer` | 글꼴 방식별로 중복된 동적 버퍼 | 공통 `TextVertexBuffer`, `TextIndexBuffer` |
| `mTextVertexCapacity`, `mTextIndexCapacity`, `UnicodeFontVertexCapacity`, `UnicodeFontIndexCapacity` | 위 중복 버퍼들의 별도 용량 추적 | 실제 버퍼 descriptor로 충분한 크기인지 확인 |
| `UnicodeFontConstantBuffer` | 초기화 시 고정된 MSDF 거리값 상수 | `MSDFConstantBuffer`를 현재 폰트 에셋의 거리값으로 갱신 |
| `FontSamplerState`, `ParticleSamplerState`, `LoadingScreenSampler` | 용도마다 별도 소유하던 샘플러 | `FSamplerStatePool` |
| `ParticleVertexBuffer`, `ParticleIndexBuffer` | 렌더러 내부 파티클 메시 | `Mesh.Particle` |
| `LoadingScreenVertexBuffer`, `LoadingScreenIndexBuffer` | 렌더러 내부 화면 쿼드 | `Mesh.Fullscreen` |
| `StrideTextured` | 제거된 개별 글꼴 경로의 stride 저장 | 공통 글꼴 함수의 `sizeof(FVertexTextured)` |
| `FEngineLoop::mDefaultFontResource` | 앱이 별도 생성·해제하던 폰트 메타데이터 | 폰트 에셋에 포함 |
| `FObjectFactory::mDefaultFontResource` | 위 메타데이터를 가리키는 비소유 raw 포인터 | `mDefaultFontAsset` 공유 참조 |
| `UNameComponent::mFontResourceRef` | 이름표가 보관하던 비소유 폰트 메타데이터 포인터 | `mFontAsset` 공유 참조 |
| `FRenderQuadInfo::TextureSRV` | 새 쿼드 파이프라인에 직접 전달하던 SRV | `Texture` 에셋 참조 |
| `FBillboardConstants`, `CBT_BillboardTexture` | 미사용 구형 빌보드 상수 구조체·버퍼 슬롯 | 제거 |
| `VST_Billboard`, `PST_Billboard` 및 생성 코드 | 미사용 구형 빌보드 셰이더 슬롯·컴파일 | 제거 |
| `ShaderTexture.hlsl`의 `billboardTextureConstants`, `billboardVS`, `billboardPS` | 위 빌보드 경로의 HLSL 구현 | 제거; 실제 메시 `mainVS/mainPS` 유지 |
| `NearTint`, `FarTint` | 앱 초기화의 사용하지 않는 색상 테스트 지역변수 | 제거 |

## 보관용 코드와 중복 작업 정리

- `Renderer.h`, `Renderer.cpp`, `GraphicsPipeline.h` 하단의 `WEEK4 renderer before WEEK3 migration` 주석 보관본을 삭제했다.
- `GraphicsManager`의 주석 처리된 옛 렌더 큐/텍스처/빌보드/하이라이트 호출부, 글자 테스트 코드, 1만 개 인스턴싱 테스트 호출을 삭제했다.
- `ObjectFactory`, `NameComponent`, `Actor`의 제거된 폰트 API를 호출하는 주석 예시도 삭제했다.
- `Renderer::releaseShader`의 중복 `FontInputLayout` 해제 블록을 제거했다.
- 매니저 소멸자의 버퍼·SRV·샘플러 수동 `Release` 루프를 제거했다. 에셋 목록/캐시 정리와 렌더러 상태 풀 정리로 대체했다.
- 앱의 직접 폰트 JSON/PNG 로딩, 프리미티브 GPU 버퍼 등록, 텍스처 경로별 직접 등록 코드를 `InitializeAssets` 호출로 교체했다.
- 에셋 테스트에서 삭제된 no-op `UnloadAsset`와 경로 기반 메타데이터 로딩을 호출하던 부분을 새 API에 맞게 수정했다.
- 새 파일은 VS 프로젝트와 필터에 등록했으며, 삭제한 WIC 파일과 `ThirdParty\DirectX` 필터도 제거했다.

## 유지한 코드의 역할

- `FFontResource`: 폰트 에셋 내부의 글리프 메트릭과 JSON 파서이다. 파일/GPU 자원을 별도로 소유하는 레거시 로더가 아니다.
- `FTextMesh`: 문자열마다 달라지는 글리프 배치/동적 정점·인덱스를 만든다. 프리미티브 헤더를 읽어 고정 메시의 인덱스를 추론하던 코드와 역할이 다르다.
- `URenderer`의 셰이더, 상수 버퍼, 선분 및 인스턴스 동적 버퍼: 현재 앱이 실제로 사용하는 그리기 상태/프레임 데이터이므로 유지했다. 텍스처·고정 메시의 로딩/소유권은 모두 에셋으로 옮겼다.
- 기존 WEEK3의 10개 `pipelines`도 유지한다. 메시·쿼드 파이프라인 모두 텍스처 에셋을 받는다.
- DirectXTK: 현재 에셋 로더가 DDS/WIC 바이트 디코딩에 사용하는 라이브러리이므로 유지했다.
- ImGui의 `malgun.ttf`와 `AddFontFromFileTTF`: 에디터 UI용 글꼴이며 ImGui DX11 백엔드가 자기 아틀라스를 만든다. 엔진의 월드 이름표/텍스처 로더와 별개여서 그대로 유지했다.
- 이미지·폰트 원본 파일은 삭제하지 않았다. 비트맵 폰트 경로도 에셋 로더 및 GPU 테스트에서 계속 검증한다.

## 검증

- `JungleEngine.sln`, Debug/x64 전체 빌드 성공.
- 텍스처 에셋 테스트 성공: 4개 실제 DDS, WIC PNG, 크기/포맷/mipmap 보존, COM 및 잘못된 입력 처리.
- 폰트 에셋 테스트 성공: ASCII 256자 메트릭, 한글 MSDF, 이미지/JSON 일치 및 잘못된 입력 처리.
- WARP/D3D11 디버그 장치 GPU 테스트 성공: 기존 10개 파이프라인, 로딩 화면, 텍스처 메시, 인스턴싱, 기즈모, 하이라이트, 파티클, bitmap/MSDF 텍스트, 실제 색상 readback.
- 에셋 중복 로딩 재사용, 캐시 제거 이후 공유 참조 수명, 실제 Actor 이름표가 폰트 에셋을 제출하는 경로 확인.
- 텍스트가 길었다가 짧아지는 경우, 빈 문자열, 잘못된 인덱스, 글꼴/메시 종류 불일치 검증.
- `git diff --check` 통과. 제거한 직접 로딩 API/소유 멤버/호출부 참조가 남아 있지 않음을 검색 확인.
- 전체 단위 테스트: 42개 중 34개 통과, 기존과 동일한 8개 실패. `FName` JSON fixture 관련 5개, 파일 관리자 경로/개행 관련 3개다. 이번 에셋 변경으로 새로 실패한 항목은 없다.
- 전체 앱의 창을 띄워 수동으로 화면을 확인한 것은 아니다. GPU 검증은 화면 없는 WARP 장치에서 수행했다.

검증 로그: `Saved/renderer-merge-build.log`, `Saved/asset-rendering-test.log`, `Saved/texture-asset-test.log`, `Saved/font-asset-test.log`, `Saved/asset-migration-unit-tests.log`.

기존 단위 테스트 실패 목록:

1. `TestActor.DeserializeClass_WhenFunctionCalled_InCorrectJson`
2. `TestFFileManager.ReadFileToString_WhenReadingFile_ReturnsCorrectContent`
3. `TestFFileManager.WriteStringToFile_WhenWritingOutsideRoot_ThrowsRuntimeError`
4. `TestFFileManager.IsUnder_WhenGivenPathIsNotUnderRoot_ReturnsFalse`
5. `TestUObject.DeseralizeClass_WhenGivenJson_DeserializeCorrectParams`
6. `TestFObjectFactory.LoadObject_WhenLoad_ReturnsCorrectObject`
7. `TestFObjectFactory.LoadObjectT_WhenLoad_ReturnsCorrectObject`
8. `TestUWorld.DeserializeClass_WhenFunctionCalled_InCorrectJson`
