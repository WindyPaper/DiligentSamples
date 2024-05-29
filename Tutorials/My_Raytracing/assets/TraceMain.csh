Texture2D DiffTextures[DIFFUSE_TEX_NUM];
SamplerState DiffTextures_sampler; // By convention, texture samplers must use the '_sampler' suffix

Texture2D SkyHDRTexture;
SamplerState SkyHDRTexture_sampler; // By convention, texture samplers must use the '_sampler' suffix

float2 DirToUV_Normalize(float3 n)
{
	n = normalize(n);
	float2 uv;

	uv.x = atan(n.z/n.x) / 3.14f;
	uv.y = acos(n.y) / ( 1.0f * 3.14f);
	return uv;
}

[numthreads(16, 16, 1)]
void TraceMain(uint3 id : SV_DispatchThreadID)
{
    uint2 pixel_pos = id.xy;

    float3 NDC = float3(pixel_pos/ScreenSize, 0.0f);
    NDC.xy = NDC.xy * 2.0f - float2(1.0f, 1.0f);

    //dx tex coordinate left top is (0, 0), so we convert to (0, 1)
    NDC.y = NDC.y * -1.0f;

    float4 PixelWPos = mul(InvViewProjMatrix, float4(NDC.xyz, 1.0f));
    PixelWPos.xyz /= PixelWPos.w;

    RayData ray;
    ray.dir = normalize(PixelWPos.xyz - CameraWPos.xyz);
    ray.o = CameraWPos.xyz;
    // float2 hit_near_far;
    float min_near = MAX_INT;
    uint hit_idx_prim = -1;
    float2 hit_coordinate = 0;
    bool back_face = false;

    RayTrace(ray, min_near, hit_idx_prim, hit_coordinate, back_face);

    if(hit_idx_prim != -1)
    {
        uint v_idx0 = MeshIdx[hit_idx_prim * 3]; 
        uint v_idx1 = MeshIdx[hit_idx_prim * 3 + 1]; 
        uint v_idx2 = MeshIdx[hit_idx_prim * 3 + 2]; 

        BVHVertex v0 = MeshVertex[v_idx0];
        BVHVertex v1 = MeshVertex[v_idx1];
        BVHVertex v2 = MeshVertex[v_idx2];

        float u = 1.0f - hit_coordinate.x - hit_coordinate.y;
        float v = hit_coordinate.x;
        float w = hit_coordinate.y;
        //float2 out_uv = v1.uv * hit_coordinate.x + v2.uv * hit_coordinate.y + (1.0f - hit_coordinate.x - hit_coordinate.y) * v0.uv;
        float2 out_uv = v0.uv * u + v1.uv * v + v2.uv * w;
        float3 out_normal = v0.normal * u + v1.normal * v + v2.normal * w;

        //get mat texture
        //float4 diff_tex_data = DiffTextures[MeshPrimData[hit_idx_prim].tex_idx].SampleLevel(DiffTextures_sampler, out_uv, 0);

        // float profile_color_intensity = float(search_num) / PROFILE_MAX_SEARCH_NUM;
        // float3 red_color = float3(1.0f, 0.0f, 0.0f);
        // float3 green_color = float3(0.0f, 1.0f, 0.0f);
        // float3 profile_color = lerp(green_color, red_color, (saturate(profile_color_intensity)));

        //OutPixel[pixel_pos] = float4(out_normal, 1.0f) * diff_tex_data;//float4(profile_color, 1.0f);

        if(back_face)
        {
            OutPixel[pixel_pos] = float4(1.0f, 0.0f, 0.0f, 1.0f);
        }
        else
        {
            OutPixel[pixel_pos] = float4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        // float3 random_color = float3(random(hit_idx_prim), random(hit_idx_prim >> 2), random(hit_idx_prim << 2));
        // OutPixel[pixel_pos] = float4(random_color, 1.0f);
    }
    else
    {
        float2 hdr_uv = DirToUV_Normalize(ray.dir);
        float4 sky_color = SkyHDRTexture.SampleLevel(SkyHDRTexture_sampler, hdr_uv, 0);

        OutPixel[pixel_pos] = sky_color;
    }
}