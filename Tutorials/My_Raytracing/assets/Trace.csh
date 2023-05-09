
#include "Common.csh"

cbuffer TraceUniformData
{
    float4x4 InvViewProjMatrix;
    float4 CameraWPos;
    float2 ScreenSize;
    float2 Padding;
}

StructuredBuffer<BVHVertex> MeshVertex;
StructuredBuffer<uint> MeshIdx;
StructuredBuffer<BVHMeshPrimData> MeshPrimData;
StructuredBuffer<BVHNode> BVHNodeData;
StructuredBuffer<BVHAABB> BVHNodeAABB;

#ifndef DIFFUSE_TEX_NUM
#   define DIFFUSE_TEX_NUM 1
#endif

Texture2D DiffTextures[DIFFUSE_TEX_NUM];
SamplerState DiffTextures_sampler; // By convention, texture samplers must use the '_sampler' suffix

RWTexture2D<float4> OutPixel;

#define kEpsilon 0.00001
#define RAY_OFFSET 0.05

static const uint PROFILE_MAX_SEARCH_NUM = 30;

float random (float num)
{
    return frac(sin((num + 12.9898)) * 43758.5453123);
}

bool RayIntersectsBox(float3 origin, float3 rayDirInv, BVHAABB aabb)
{
	const float3 t0 = (aabb.lower.xyz - origin) * rayDirInv;
	const float3 t1 = (aabb.upper.xyz - origin) * rayDirInv;

	const float3 tmax = max(t0, t1);
	const float3 tmin = min(t0, t1);

	const float a1 = min(tmax.x, min(tmax.y, tmax.z));
	const float a0 = max( max(tmin.x,tmin.y), max(tmin.z, 0.0f) );

	return a1 >= a0;
}

//Adapted from https://github.com/kayru/RayTracedShadows/blob/master/Source/Shaders/RayTracedShadows.comp
bool RayTriangleIntersect(
    uint hit_idx_prim,
	const float3 orig,
	const float3 dir,
	inout float t,
	inout float2 bCoord)
{
    uint v_idx0 = MeshIdx[hit_idx_prim * 3]; 
    uint v_idx1 = MeshIdx[hit_idx_prim * 3 + 1]; 
    uint v_idx2 = MeshIdx[hit_idx_prim * 3 + 2]; 

    BVHVertex v0 = MeshVertex[v_idx0];
    BVHVertex v1 = MeshVertex[v_idx1];
    BVHVertex v2 = MeshVertex[v_idx2];

    float3 e0 = (v1.pos - v0.pos);
    float3 e1 = (v2.pos - v0.pos);
    float3 v0_pos = v0.pos;

	const float3 s1 = cross(dir.xyz, e1);
	const float  invd = 1.0 / (dot(s1, e0));
	const float3 d = orig.xyz - v0_pos;
	bCoord.x = dot(d, s1) * invd;
	const float3 s2 = cross(d, e0);
	bCoord.y = dot(dir.xyz, s2) * invd;
	t = dot(e1, s2) * invd;

	if (
//#if BACKFACE_CULLING
		//dot(s1, e0) < -kEpsilon ||
//#endif
		bCoord.x < 0.0 || bCoord.x > 1.0 || bCoord.y < 0.0 || (bCoord.x + bCoord.y) > 1.0 || t < 0.0 || t > 1e9)
	{
		return false;
	}
	else
	{
		return true;
	}
}

void FixedRcpInf(inout float3 RayDirInv)
{
    if(isinf(RayDirInv.x))
    {
        RayDirInv.x = MAX_INT;
    }

    if(isinf(RayDirInv.y))
    {
        RayDirInv.y = MAX_INT;
    }

    if(isinf(RayDirInv.z))
    {
        RayDirInv.z = MAX_INT;
    }
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

    float3 RayDir = normalize(PixelWPos.xyz - CameraWPos.xyz);
    float3 RayOri = CameraWPos.xyz;
    float2 hit_near_far;
    float min_near = MAX_INT;
    uint hit_idx_prim = -1;

    // bool collision = false;
    float3 RayDirInv = rcp(RayDir);
    FixedRcpInf(RayDirInv);
    // collision = RayIntersectsBox(RayOri, ray_dir_inv, BVHNodeAABB[0].lower.xyz, BVHNodeAABB[0].upper.xyz);

    float hit_min = MAX_INT;
    uint hit_triangle_idx = -1;
    float2 hit_coordinate = 0;
    uint search_num = 0;

    //trace bvh
    uint stack[128];
    int curr_idx = 0;
    stack[curr_idx] = 0;
    while(curr_idx >= 0)
    {        

        uint node_idx = stack[curr_idx];
        --curr_idx;

        const uint L_idx = BVHNodeData[node_idx].left_idx;
        const uint R_idx = BVHNodeData[node_idx].right_idx;

        if(RayIntersectsBox(RayOri, RayDirInv, BVHNodeAABB[L_idx]))
        {
            if(BVHNodeData[L_idx].object_idx != 0xFFFFFFFFu) // leaf
            {
                //triangle intersection                
                uint t_hit_prim = BVHNodeData[L_idx].object_idx;                

                float t_min;
                float2 t_coord;
                if(RayTriangleIntersect(t_hit_prim, RayOri, RayDir, t_min, t_coord))
                {
                    ++search_num;
                    // hit_idx_prim = t_hit_prim;//test

                    if(t_min < hit_min)
                    {
                        hit_min = t_min;
                        hit_coordinate = t_coord;
                        hit_idx_prim = t_hit_prim;
                    }
                }
            }
            else // internal node
            {
                ++curr_idx;
                stack[curr_idx] = L_idx;
            }
        }

        if(RayIntersectsBox(RayOri, RayDirInv, BVHNodeAABB[R_idx]))
        {
            if(BVHNodeData[R_idx].object_idx != 0xFFFFFFFFu) // leaf
            {
                //triangle intersection
                uint t_hit_prim = BVHNodeData[R_idx].object_idx;                

                float t_min;
                float2 t_coord;
                if(RayTriangleIntersect(t_hit_prim, RayOri, RayDir, t_min, t_coord))
                {
                    ++search_num;
                    // hit_idx_prim = t_hit_prim;//test

                    if(t_min < hit_min)
                    {
                        hit_min = t_min;
                        hit_coordinate = t_coord;
                        hit_idx_prim = t_hit_prim;
                    }
                }
            }
            else // internal node
            {
                ++curr_idx;
                stack[curr_idx] = R_idx;
            }
        }
    }

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

        //get mat texture
        float4 diff_tex_data = DiffTextures[MeshPrimData[hit_idx_prim].tex_idx].SampleLevel(DiffTextures_sampler, out_uv, 0);

        float profile_color_intensity = float(search_num) / PROFILE_MAX_SEARCH_NUM;
        float3 red_color = float3(1.0f, 0.0f, 0.0f);
        float3 green_color = float3(0.0f, 1.0f, 0.0f);
        float3 profile_color = lerp(green_color, red_color, (saturate(profile_color_intensity)));

        OutPixel[pixel_pos] = diff_tex_data;//float4(profile_color, 1.0f);
        // float3 random_color = float3(random(hit_idx_prim), random(hit_idx_prim >> 2), random(hit_idx_prim << 2));
        // OutPixel[pixel_pos] = float4(random_color, 1.0f);
    }
    else
    {
        OutPixel[pixel_pos] = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}