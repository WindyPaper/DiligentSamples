
#include "Common.csh"

struct BVHVertex
{
    float3 pos;
    float2 uv;
};

cbuffer TraceUniformData
{
    float4x4 InvViewProjMatrix;
    float4 CameraWPos;
    float2 ScreenSize;
    float2 Padding;
}

StructuredBuffer<BVHVertex> MeshVertex;
StructuredBuffer<uint> MeshIdx;
StructuredBuffer<BVHNode> BVHNodeData;
StructuredBuffer<BVHAABB> BVHNodeAABB;

RWTexture2D<float4> OutPixel;

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
	const float3 orig,
	const float3 dir,
	float3 v0,
	float3 e0,
	float3 e1,
	inout(float) t,
	inout(float2) bCoord)
{
	const float3 s1 = cross(dir.xyz, e1);
	const float  invd = 1.0 / (dot(s1, e0));
	const float3 d = orig.xyz - v0;
	bCoord.x = dot(d, s1) * invd;
	const float3 s2 = cross(d, e0);
	bCoord.y = dot(dir.xyz, s2) * invd;
	t = dot(e1, s2) * invd;

	if (
//#if BACKFACE_CULLING
		dot(s1, e0) < -kEpsilon ||
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

[numthreads(8, 8, 1)]
void TraceMain(uint3 id : SV_DispatchThreadID)
{
    uint2 pixel_pos = id.xy;

    float3 NDC = float3(pixel_pos/ScreenSize, 0.5f);
    NDC.xy = NDC.xy * 2.0f - float2(1.0f, 1.0f);
    float4 PixelWPos = mul(InvViewProjMatrix, float4(NDC.xyz, 1.0f));
    PixelWPos.xyz /= PixelWPos.w;

    float3 RayDir = normalize(PixelWPos.xyz - CameraWPos.xyz);
    float3 RayOri = CameraWPos.xyz;
    float2 hit_near_far;
    float min_near = MAX_INT;
    uint hit_idx_prim = -1;

    // bool collision = false;
    float3 RayDirInv = rcp(RayDir);
    // collision = RayIntersectsBox(RayOri, ray_dir_inv, BVHNodeAABB[0].lower.xyz, BVHNodeAABB[0].upper.xyz);

    //trace bvh
    uint stack[64];
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
                //...

                hit_idx_prim = BVHNodeData[L_idx].object_idx;//test
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
                //...

                hit_idx_prim = BVHNodeData[R_idx].object_idx;//test
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
        OutPixel[pixel_pos] = float4(1.0f, 0.0f, 0.0f, 1.0f);
    }
    else
    {
        OutPixel[pixel_pos] = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}