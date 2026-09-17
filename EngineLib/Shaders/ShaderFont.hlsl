cbuffer constants : register(b0)
{
    float3 Location;
    float3 Scale;

    float3 CameraRight;
    float3 CameraUp;
    
    row_major float4x4 ViewProjection;
    float4 Tint;
}

Texture2D FontAtlas : register(t0);
SamplerState FontSampler : register(s0);

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
    const float4 atlasColor = FontAtlas.Sample(FontSampler, input.texCoord);

    /*
    float brightness = max(atlasColor.r, max(atlasColor.g, atlasColor.b));
    brightness *= atlasColor.a;
    clip(brightness - 0.01f);
    
    // Alpha 값이 거의 0인 픽셀은 렌더링하지 않는다
    // float alpha = atlasColor.a * Tint.a;;
   // clip(alpha - 0.001f);
    
    return float4(Tint.rgb, Tint.a);
*/

     // 검은 배경 + 밝은 글자인 현재 아틀라스 기준
    float coverage = min(atlasColor.r, min(atlasColor.g, atlasColor.b));
    float alpha = saturate(coverage * atlasColor.a * Tint.a);

    clip(alpha - 0.001f);

    return float4(Tint.rgb, alpha);
}
