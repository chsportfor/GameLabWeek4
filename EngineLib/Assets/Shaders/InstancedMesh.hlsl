cbuffer Camera : register(b0) { row_major float4x4 ViewProjection; }
struct Instance { row_major float4x4 World; float4 Tint; };
StructuredBuffer<Instance> Instances : register(t1);
struct VS_INPUT { float3 position : POSITION; float4 color : COLOR; float2 uv : TEXCOORD; };
struct PS_INPUT { float4 position : SV_POSITION; float4 color : COLOR; };
PS_INPUT mainVS(VS_INPUT input, uint id : SV_InstanceID)
{
    PS_INPUT output;
    Instance instance = Instances[id];
    output.position = mul(mul(float4(input.position, 1), instance.World), ViewProjection);
    output.color = float4(lerp(input.color.rgb, instance.Tint.rgb, instance.Tint.a), 1);
    return output;
}
float4 mainPS(PS_INPUT input) : SV_TARGET { return input.color; }
