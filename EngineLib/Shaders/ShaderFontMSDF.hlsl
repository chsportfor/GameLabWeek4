// ShaderFontMSDF.hlsl
cbuffer constants : register(b0)
{
    // row_major float4x4 World;
    float3 Location;
    float3 Scale;

    float3 CameraRight;
    float3 CameraUp;
    
    row_major float4x4 ViewProjection;
    float4 Tint;
}

cbuffer UnicodeFontConstants : register(b1)
{
    float DistanceRange;
    float3 Padding;
};

Texture2D msdfAtlas : register(t0);
SamplerState msdfAtlasSampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

// RGB 중앙값
float Median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

float ScreenPxRange(float2 uv)
{
    uint width;
    uint height;
    // 텍스처 크기 정보
    msdfAtlas.GetDimensions(width, height);

    // KoreanFullAtlas.json의 atlas.distanceRange
    //const float distanceRange = 4.0f;

    // 확대·축소에 맞춰 경계 조절
    float2 unitRange = float2(DistanceRange, DistanceRange)
                / float2(width, height);

    // 화면 한 픽셀 이동 시 UV가 변하는 양
    // 현재 렌더링 크기를 알아낸다
    // fwidth(x) = abs(ddx(x)) + abs(ddy(x))
    // uvPerPixel = 0 방지

    float2 uvPerPixel = max(fwidth(uv), float2(0.000001f, 0.000001f));

    // U를 1.0만큼 이동하려면 화면에서 몇 pixel을 이동해야하는지 계산
    // UV 좌표계의 단위 길이가 현재 화면에서 대략 몇 픽셀 크기
    float2 screenTexSize = 1.0f / uvPerPixel;

    // MSDF의 거리 범위를 현재 화면의 픽셀 단위로 환산
    // unitRange.x * screenTexSize.x + unitRange.y * screenTexSize.y
    // 0.5: X와 Y 결과의 평균내기 위해 나누기 2
    // 공식 msdfgen 설명에서도 screenPxRange가 1보다 작아지면 안 된다고 함
    return max(0.5f * dot(unitRange, screenTexSize), 1.0f);
}


// 기존과 동일
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    // output.position = mul(mul(float4(input.position, 1.0f), World), ViewProjection);

    float2 corner = input.position.yz;
    float2 scale = Scale.yz;

    float3 worldPosition =
        Location
        + CameraRight * corner.x * scale.x
        + CameraUp * corner.y * scale.y;

    output.position = mul(float4(worldPosition, 1.0f), ViewProjection);
    
    output.texCoord = input.texCoord;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float3 msdf = msdfAtlas.Sample(msdfAtlasSampler, input.texCoord).rgb;

    float distance = Median(msdf.r, msdf.g, msdf.b);

    // 먼저 경계값 0.5를 빼서 경계를 0으로 설정
    float screenDistance =  ScreenPxRange(input.texCoord) * (distance - 0.5f);

    // 글자 바깥 => 0: 투명
    // 글자 경계 => 중간값:
    // 글자 안쪽 =>1 불투명
    // saturate()로 값을 0~1 제한
    float coverage = saturate(screenDistance + 0.5f);

    float alpha = coverage * Tint.a;

    clip(alpha - 0.001f);

    return float4(Tint.rgb, alpha);
}


