Texture2D Texture : register(t0);
SamplerState TextureSampler : register(s0);

cbuffer particleConstants : register(b0)
{
    float3 Location;
    float3 Scale;
    
    row_major float4x4 ViewProjection;

    float3 CameraRight;
    float3 CameraUp;

    float4 Tint;

    int numRows;
    int numCols;
    int currentFrame;
    int nextFrame;
    
    float frameRatio;
}

struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 currentUV : TEXCOORD0;
    float2 nextUV : TEXCOORD1;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    // Quad is defined in yz-plane
    float2 corner = input.position.yz;
    float2 scale = Scale.yz;

    float3 worldPosition =
        Location
        + CameraRight * corner.x * scale.x
        + CameraUp * corner.y * scale.y;

    output.position = mul(float4(worldPosition, 1.0f), ViewProjection);

    output.currentUV[0] = (input.uv.x + currentFrame % numCols) / numCols;
    output.currentUV[1] = (input.uv.y + currentFrame / numCols) / numRows;

    output.nextUV[0] = (input.uv.x + nextFrame % numCols) / numCols;
    output.nextUV[1] = (input.uv.y + nextFrame / numCols) / numRows;

    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 currentColor = Texture.Sample(TextureSampler, input.currentUV);
    float4 nextColor = Texture.Sample(TextureSampler, input.nextUV);
    float4 finalColor = lerp(currentColor, nextColor, frameRatio);
    
    finalColor *= Tint;
    
    return finalColor;
}
