
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

// compute the near and far intersections of the cube (stored in the x and y components) using the slab method
// no intersection means vec.x > vec.y (really tNear > tFar)
uint IntersectAABB(float3 rayOrigin, float3 rayDir, const in BVHAABB aabb) 
{
    float3 tMin = (aabb.lower.xyz - rayOrigin) / rayDir;
    float3 tMax = (aabb.upper.xyz - rayOrigin) / rayDir;
    float3 t1 = min(tMin, tMax);
    float3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    //near_far = float2(tNear, tFar);
    return tFar >= tNear;
};

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

        if(IntersectAABB(RayOri, RayDir, BVHNodeAABB[L_idx]))
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

        if(IntersectAABB(RayOri, RayDir, BVHNodeAABB[R_idx]))
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