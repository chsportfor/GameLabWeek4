Texture2D Texture : register(t0);
SamplerState TextureSampler : register(s0);

cbuffer textureConstants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 ViewProjection;
    float4 Tint; // rgb = 덧입힐 색, a = 섞는 비율(0 이면 정점 색 그대로)

    // sub uv
    float2 UVScale;
    float2 UVOffset;
}

struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    output.position = mul(mul(float4(input.position.xyz, 1.0f), World), ViewProjection);
    output.uv = input.uv * UVScale + UVOffset;

    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 color = Texture.Sample(TextureSampler, input.uv);
    return color * Tint;
}
