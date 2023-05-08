
#include "Common.csh"

// struct BVHVertex
// {
//     float3 pos;
//     float2 uv;
// };

StructuredBuffer<BVHVertex> MeshVertex;
StructuredBuffer<uint> MeshIdx;

RWStructuredBuffer<BVHAABB> OutAABB; //size = num_all_nodes
RWStructuredBuffer<float3> OutPrimCentroid;

[numthreads(64, 1, 1)]
void GenerateAABBMain(uint3 id : SV_DispatchThreadID)
{
    uint prim_idx = id.x;

    if(prim_idx >= num_objects)
    {
        return;
    }

    BVHAABB aabb;
    aabb.upper = float4(MIN_INT, MIN_INT, MIN_INT, 0.0f);
    aabb.lower = float4(MAX_INT, MAX_INT, MAX_INT, 0.0f);    

    uint start_idx = prim_idx * 3;
    float3 prim_centroid = float3(0.0f, 0.0f, 0.0f);
    for(int i = 0; i < 3; ++i)
    {
        uint curr_vertex_idx = MeshIdx[start_idx + i];
        BVHVertex v = MeshVertex[curr_vertex_idx];

        aabb.upper.xyz = max(v.pos, aabb.upper.xyz);
        aabb.lower.xyz = min(v.pos, aabb.lower.xyz);

        prim_centroid += v.pos;
    }
    prim_centroid /= 3.0f;

    OutAABB[num_interal_nodes + prim_idx] = aabb;
    OutPrimCentroid[prim_idx] = prim_centroid;
}