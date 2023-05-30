
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
	inout float2 bCoord,
    inout bool back_face)
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
#if BACKFACE_CULLING
		dot(s1, e0) < -kEpsilon ||
#endif
		bCoord.x < 0.0 || bCoord.x > 1.0 || bCoord.y < 0.0 || (bCoord.x + bCoord.y) > 1.0 || t < 0.0 || t > 1e9)
	{
		return false;
	}
	else
	{
        if(dot(s1, e0) < -kEpsilon)
        {
            back_face = true;
        }
        else
        {
            back_face = false;
        }
        
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


struct RayData
{
    float3 o;
    float3 dir;
};

void RayTrace(RayData ray, inout float hit_min, inout uint hit_idx_prim, inout float2 hit_coordinate, inout bool back_face)
{
    float3 RayDirInv = rcp(ray.dir);
    FixedRcpInf(RayDirInv);
    // collision = RayIntersectsBox(RayOri, ray_dir_inv, BVHNodeAABB[0].lower.xyz, BVHNodeAABB[0].upper.xyz);

    // float hit_min = MAX_INT;
    // uint hit_triangle_idx = -1;
    // float2 hit_coordinate = 0;
    // uint search_num = 0;

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

        if(RayIntersectsBox(ray.o, RayDirInv, BVHNodeAABB[L_idx]))
        {
            if(BVHNodeData[L_idx].object_idx != 0xFFFFFFFFu) // leaf
            {
                //triangle intersection                
                uint t_hit_prim = BVHNodeData[L_idx].object_idx;                

                float t_min;
                float2 t_coord;
                bool t_back_face = false;
                if(RayTriangleIntersect(t_hit_prim, ray.o, ray.dir, t_min, t_coord, t_back_face))
                {
                    // ++search_num;
                    // hit_idx_prim = t_hit_prim;//test

                    if(t_min < hit_min)
                    {
                        hit_min = t_min;
                        hit_coordinate = t_coord;
                        hit_idx_prim = t_hit_prim;
                        back_face = t_back_face;
                    }
                }
            }
            else // internal node
            {
                ++curr_idx;
                stack[curr_idx] = L_idx;
            }
        }

        if(RayIntersectsBox(ray.o, RayDirInv, BVHNodeAABB[R_idx]))
        {
            if(BVHNodeData[R_idx].object_idx != 0xFFFFFFFFu) // leaf
            {
                //triangle intersection
                uint t_hit_prim = BVHNodeData[R_idx].object_idx;                

                float t_min;
                float2 t_coord;
                bool t_back_face = false;
                if(RayTriangleIntersect(t_hit_prim, ray.o, ray.dir, t_min, t_coord, t_back_face))
                {
                    // ++search_num;
                    // hit_idx_prim = t_hit_prim;//test

                    if(t_min < hit_min)
                    {
                        hit_min = t_min;
                        hit_coordinate = t_coord;
                        hit_idx_prim = t_hit_prim;
                        back_face = t_back_face;
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
}

#include "TraceMain.csh"
#include "GenVertexAORaysMain.csh"
#include "GenVertexAOColorMain.csh"
#include "GenTriangleAORaysMain.csh"
#include "GenTriangleAOPosMain.csh"