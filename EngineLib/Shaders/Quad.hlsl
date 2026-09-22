cbuffer Constants : register(b0)
{
	row_major matrix model;
	float4 color;
    float4 sub_uv;
    float4 next_sub_uv;
    float frame_blend;
    int has_texture;
    int grayscale_mode;
    int padding;
}

cbuffer CameraConstants : register(b1)
{
    row_major matrix view_projection;
}

struct PS_INPUT
{
	float4 position : SV_POSITION;
    float2 current_uv : TEXCOORD0;
    float2 next_uv : TEXCOORD1;
};

Texture2D default_texture : register(t0);
SamplerState default_sampler : register(s0);

// Vertex Shader
PS_INPUT mainVS(uint vertex_id : SV_VertexID)
{
	PS_INPUT output;
	
    float3 vertices[4] = { 
		float3(0.0, -0.5f, 0.5f), 
		float3(0.0, 0.5f, 0.5f), 
		float3(0.0, 0.5f, -0.5f),
		float3(0.0, -0.5f, -0.5f) 
	};
	
    float2 uv[4] = { 
		float2(0.0f, 0.0f), 
		float2(1.0f, 0.0f), 
		float2(1.0f, 1.0f), 
		float2(0.0f, 1.0f) 
	};
	
    int indices[6] = { 0, 1, 2, 0, 2, 3 };
    
    float4 local_position = float4(vertices[indices[vertex_id]], 1.0f);
	
	output.position = mul(mul(local_position, model), view_projection);
    output.current_uv = uv[indices[vertex_id]] * sub_uv.zw + sub_uv.xy;
    output.next_uv = uv[indices[vertex_id]] * next_sub_uv.zw + next_sub_uv.xy;
	
	return output;
}

// Pixel Shader
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 final_color = color;
    if (has_texture == 1)
    {
        float4 tex_color = default_texture.Sample(default_sampler, input.current_uv);
        if (frame_blend > 0.0f)
        {
            float4 next_color = default_texture.Sample(default_sampler, input.next_uv);
            tex_color = lerp(tex_color, next_color, saturate(frame_blend));
        }
		
        if (grayscale_mode == 1)
        {
            final_color.a = tex_color.r;
        }
		else
        {
            final_color = tex_color * color;
        }
    }
	
    return final_color;
}
