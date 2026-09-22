cbuffer TextConstants : register(b0)
{
    float3 Location; float3 Scale;
    float3 CameraRight; float3 CameraUp;
    row_major float4x4 ViewProjection;
    float4 Tint;
}
cbuffer AtlasConstants : register(b1) { float DistanceRange; int IsMSDF; float2 Padding; }
Texture2D Atlas : register(t0);
SamplerState AtlasSampler : register(s0);
struct VS_INPUT { float3 position : POSITION; float2 uv : TEXCOORD; };
struct PS_INPUT { float4 position : SV_POSITION; float2 uv : TEXCOORD; };
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    float3 world = Location + CameraRight * input.position.y * Scale.y + CameraUp * input.position.z * Scale.z;
    output.position = mul(float4(world, 1), ViewProjection);
    output.uv = input.uv;
    return output;
}
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 sampleColor = Atlas.Sample(AtlasSampler, input.uv);
    float coverage;
    if (IsMSDF)
    {
        float distance = max(min(sampleColor.r, sampleColor.g), min(max(sampleColor.r, sampleColor.g), sampleColor.b));
        uint width, height;
        Atlas.GetDimensions(width, height);
        float2 unitRange = DistanceRange / float2(width, height);
        float2 screenTexSize = 1 / max(fwidth(input.uv), float2(0.000001, 0.000001));
        float range = max(0.5 * dot(unitRange, screenTexSize), 1);
        coverage = saturate(range * (distance - 0.5) + 0.5);
    }
    else coverage = min(sampleColor.r, min(sampleColor.g, sampleColor.b)) * sampleColor.a;
    float alpha = saturate(coverage * Tint.a);
    clip(alpha - 0.001);
    return float4(Tint.rgb, alpha);
}
