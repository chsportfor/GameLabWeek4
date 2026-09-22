struct LineData
{
    float4 color;
    float3 start;
    float thickness;
    float3 end;
    float padding;
};

cbuffer CameraConstants : register(b0)
{
    row_major float4x4 view_projection;
    float2 viewport_size;
    float padding[2];
}

StructuredBuffer<LineData> lines : register(t0);

struct PS_INPUT
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

float2 Perpendicular(float2 v)
{
    return float2(-v.y, v.x);
}

// Vertex Shader
PS_INPUT mainVS(uint instance_id : SV_InstanceID, uint vertex_id : SV_VertexID)
{
	PS_INPUT output;

    LineData current_line = lines[instance_id];

    float4 proj_start = mul(float4(current_line.start, 1.f), view_projection);
    float4 proj_end = mul(float4(current_line.end, 1.f), view_projection);
    // Clip before the perspective divide. The finite editor grid can cross the camera plane.
    output.color = current_line.color;
    output.position = float4(0, 0, -1, 1);
    const float nearZ = 0.000001f;
    if (proj_start.z < nearZ && proj_end.z < nearZ) return output;
    if (proj_start.z < nearZ)
        proj_start = lerp(proj_start, proj_end, (nearZ - proj_start.z) / (proj_end.z - proj_start.z));
    if (proj_end.z < nearZ)
        proj_end = lerp(proj_end, proj_start, (nearZ - proj_end.z) / (proj_start.z - proj_end.z));
    float3 proj_start_ndc = proj_start.xyz / proj_start.w;
    float3 proj_end_ndc = proj_end.xyz / proj_end.w;
    float2 pixel_delta = (proj_end_ndc.xy - proj_start_ndc.xy) * viewport_size * 0.5f;
    float pixel_length = length(pixel_delta);
    if (pixel_length < 0.000001f) return output;
    float2 normal = Perpendicular(pixel_delta) / pixel_length;
    float2 offset_pixels = normal * current_line.thickness * 0.5f;
    float2 offset = offset_pixels * 2.0f / viewport_size;

    float4 start_top = proj_start;
    float4 start_bottom = proj_start;
    float4 end_top = proj_end;
    float4 end_bottom = proj_end;

    start_top.xy += offset * proj_start.w;
    start_bottom.xy -= offset * proj_start.w;
    end_top.xy += offset * proj_end.w;
    end_bottom.xy -= offset * proj_end.w;

    float4 positions[4] =
    {
        start_top,
        end_top,
        end_bottom,
        start_bottom
    };

    int indices[6] = { 0, 1, 2, 0, 2, 3 };

    output.position = positions[indices[vertex_id]];
    output.color = current_line.color;

	return output;
}

// Pixel Shader
float4 mainPS(PS_INPUT input) : SV_TARGET
{
	return input.color;
}
