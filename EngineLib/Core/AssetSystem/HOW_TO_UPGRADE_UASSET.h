#pragma once

/*
해야 할 일 — UMaterial에 Roughness를 추가하는 버전 1 → 2 예시

1. 정책 결정: 신규 애셋 기본값 0.8, 기존 v1 파일의 변환값 0.5, 허용 범위 [0, 1].
2. Material.h의 UMaterial과 MaterialAssetFile.h의 FMaterial_uasset에 멤버 추가.
3. MaterialAssetFile.cpp의 Serialize / DeserializeMaterial / Validate 수정.
4. 같은 파일의 GetMaterialFileSchema().LatestVersion을 2로 변경.
5. UpgradeMaterialToLatest에 1→2 변환 추가. 기존 변환 단계는 삭제하지 않기.
6. UMaterial::Load에서 값 복원, GetDeclaredProperties에 프로퍼티 등록.
7. 참조 속성이 바뀌면 RebuildDependencies 수정. Roughness 추가에는 변경 불필요.
8. MaterialAssetFile.h의 파일 형식 주석에 스키마 버전·필드·기본값·범위 갱신.
9. 구버전 변환·신규 저장·실제 로드·재스캔·실패 시 원본 보존 테스트.

FAssetFileDocument = 업그레이드 중 사용하는 Header + 수정 가능한 JSON Body + 바이너리 Payload.
FMaterial_uasset = 현재 버전의 파일을 해석한 값. UMaterial = 실행 중 사용하는 애셋 객체.
공용 ContainerVersion은 1 유지. 기존 등록 매크로와 매니저는 수정할 필요 없음.
임포터는 FMaterial_uasset의 기본값을 사용하므로 인수 변경 없이 신규 파일에 0.8을 저장함.
임포트 시 별도 값을 받으려면 임포터 인수와 File.Roughness 대입을 추가할 것.

아래는 각 파일에 적용할 예시 조각이다. 실제 Roughness 기능이나 버전 2를 활성화하지 않는다.
*/

#if 0 // 문서 전용: 아래 코드는 빌드하지 않는다.

// [2] Asset/Material.h — UMaterial의 public 멤버에 추가.
float Roughness = 0.8f;

// [2] AssetFile/MaterialAssetFile.h — FMaterial_uasset의 멤버에 추가.
float Roughness = 0.8f;

// [3] AssetFile/MaterialAssetFile.cpp — Serialize의 마지막 return을 교체.
// Header/Texture는 기존 코드가 준비한 값이며, using namespace Detail 범위 안이다.
return WriteDocument(Header, Json{
    "DiffuseColor", FloatArray({
        File.DiffuseColor.R, File.DiffuseColor.G,
        File.DiffuseColor.B, File.DiffuseColor.A}),
    "DiffuseTexture", Texture,
    "Roughness", File.Roughness
});

// [3] DeserializeMaterial — 기존 Validate(File) 호출 전에 추가.
// v2에서 키가 없으면 구조체 기본값 0.8 유지. 키가 있지만 타입이 틀리면 실패.
if (Body.hasKey("Roughness"))
{
    File.Roughness = Float(Body.at("Roughness"));
}

// [3] Validate(const FMaterial_uasset& File) — 기존 검증에 추가.
if (!std::isfinite(File.Roughness) || File.Roughness < 0.0f || File.Roughness > 1.0f)
{
    throw std::runtime_error("Material roughness must be within [0, 1]");
}

// [4, 7] GetMaterialFileSchema — 첫 번째 값만 1에서 2로 변경.
// 나머지 두 콜백은 기존 코드 그대로다. 전체 형태를 확인하기 위해 함께 표시한다.
const FAssetFileSchema& AssetFile::GetMaterialFileSchema()
{
    static const FAssetFileSchema Schema{
        2, // LatestVersion. Serialize의 기록 버전과 Deserialize의 요구 버전도 이 값을 사용.
        &UpgradeMaterialToLatest,
        [](FAssetFileDocument& Document) // RebuildDependencies
        {
            Document.Header.Dependencies.Empty();
            if (Document.Body.hasKey("DiffuseTexture") && !Document.Body.at("DiffuseTexture").IsNull())
            {
                const auto Path = Detail::String(Document.Body.at("DiffuseTexture"));
                if (!Path.empty()) Document.Header.Dependencies.Add(FString(Path));
            }
        },
        [](const FAssetFileDocument& Document) // Validate: UObject/GPU 생성 없이 검사.
        {
            const auto Bytes = Detail::WriteDocument(Document.Header, Document.Body,
                {Document.Payload.GetData(), size_t(Document.Payload.Num())});
            (void)DeserializeMaterial({Bytes.GetData(), size_t(Bytes.Num())});
        }
    };
    return Schema;
}

// [5] UpgradeMaterialToLatest — 기존 v1 파일에 0.5를 명시적으로 기록한다.
// 버전은 변환을 마친 뒤 올린다. 다음 v3에서는 아래 if 뒤에 2→3 if를 추가한다.
void AssetFile::UpgradeMaterialToLatest(FAssetFileDocument& Document)
{
    if (Document.Header.SchemaVersion == 1)
    {
        if (!Document.Body.hasKey("Roughness"))
        {
            Document.Body["Roughness"] = 0.5f;
        }
        Document.Header.SchemaVersion = 2;
    }
}

// [6] Asset/Material.cpp — UMaterial::Load의 기존 DiffuseColor 대입 뒤에 추가.
Roughness = File.Roughness;

// [6] UMaterial::GetDeclaredProperties — Properties 배열을 다음과 같이 변경.
// 이 목록은 UObject 리플렉션용이다. .uasset의 Serialize/Deserialize를 자동화하지 않는다.
static const FPropertyInfo Properties[] = {
    REFLECT_PROPERTY(UMaterial, DiffuseColor),
    REFLECT_PROPERTY(UMaterial, DiffuseTexture),
    REFLECT_PROPERTY(UMaterial, Roughness)
};

// [8] MaterialAssetFile.h — 파일 형식 주석에 반영할 내용.
/*
전체 파일: "UAJS"[4] | uint32 LE ContainerVersion=1 | uint64 LE H | uint64 LE B
           | UTF-8 헤더 JSON[H] | UTF-8 본문 JSON[B] | EOF
H/B는 실제 JSON 바이트 길이. WriteDocument가 다시 계산한다. 바이너리 본문 없음.

헤더:
{
    "AssetType": "UMaterial",
    "SchemaVersion": 2,
    "Standalone": true,
    "Dependencies": ["Textures/Body.uasset"]
}

본문 (기존 v1 파일을 변환한 결과):
{
    "DiffuseColor": [1.0, 0.5, 0.2, 1.0],
    "DiffuseTexture": "Textures/Body.uasset",
    "Roughness": 0.5
}

Roughness: 유한한 실수 [0, 1]. 신규 파일 기본값 / v2에서 키 생략 시 기본값은 0.8.
v1 파일은 UpgradeMaterialToLatest가 키가 없을 때 0.5를 추가하여 v2로 변환한다.
색상·텍스처·의존성의 기존 형식과 규칙도 주석에 유지한다.
*/


#endif

/*
코드 반영 후 자동 실행 경로 (별도로 호출 코드를 추가할 필요 없음):

ScanAssets / RegisterAsset
  → ReadHeader: 파일 버전 확인
  → GetSchema: 클래스별 최신 버전 조회
  → UpgradeFile: 최신이면 즉시 반환, 미래 버전이면 실패
    → 구버전 파일을 FAssetFileDocument로 읽기
    → UpgradeMaterialToLatest → 의존성 재생성 → 검증
    → 임시 파일 작성·재독해 검증 → 원본 변경 여부 확인
    → 원본 백업 → 파일 교체
  → 갱신된 헤더로 메타정보·역참조 맵 등록

실제 애셋 요청
  → LoadAsset → UMaterial 생성 → Load → DeserializeMaterial → 멤버 대입

파일별 교체이며 스캔 전체의 디스크 롤백은 아니다. 뒤 파일에서 실패해도 이미 변환한 파일은 유지한다.
스캔 실패 시 새 인덱스는 적용하지 않는다. 이미 로드된 객체의 자동 재로드도 수행하지 않는다.
셰이더에서 Roughness를 사용하려면 렌더 정보·상수 버퍼·셰이더 연결을 별도로 구현해야 한다.
실행 중 UMaterial의 값을 바꾸는 것만으로 .uasset이 자동 저장되지는 않는다.
*/
