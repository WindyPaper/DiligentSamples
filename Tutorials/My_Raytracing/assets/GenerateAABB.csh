
#include "Common.csh"

struct BVHVertex
{
    float3 pos;
    float2 uv;
};

static const int MAX_INT = 2147483647;
static const int MIN_INT = -2147483648;

StructuredBuffer<BVHVertex> MeshVertex;
StructuredBuffer<uint> MeshIdx;

RWStructuredBuffer<BVHAABB> OutAABB; //size = num_all_nodes

[numthreads(8, 8, 1)]
void GenerateAABBMain(uint3 id : SV_DispatchThreadID)
{
    uint prim_idx = id.x * id.y;

    if(prim_idx >= num_objects)
    {
        return;
    }

    BVHAABB aabb;
    aabb.upper = float4(MIN_INT, MIN_INT, MIN_INT, 0.0f);
    aabb.lower = float4(MAX_INT, MAX_INT, MAX_INT, 0.0f);    

    uint start_idx = prim_idx * 3;
    for(int i = 0; i < 3; ++i)
    {
        uint curr_vertex_idx = MeshIdx[start_idx + i];
        BVHVertex v = MeshVertex[curr_vertex_idx];

        aabb.upper.xyz = max(v.pos, aabb.upper.xyz);
        aabb.lower.xyz = min(v.pos, aabb.lower.xyz);
    }

    OutAABB[num_interal_nodes + prim_idx] = aabb;
}