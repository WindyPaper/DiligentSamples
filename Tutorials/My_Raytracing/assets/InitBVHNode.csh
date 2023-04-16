
#include "Common.csh"

// struct BVHVertex
// {
//     float3 pos;
//     float2 uv;
// };


StructuredBuffer<uint> InIdxData;
StructuredBuffer<BVHAABB> InAABBData;

RWStructuredBuffer<BVHNode> OutBVHNodeData;
RWStructuredBuffer<BVHAABB> OutReorderAABBData;

[numthreads(64, 1, 1)]
void InitBVHNodeMain(uint3 id : SV_DispatchThreadID)
{
    uint node_idx = id.x;

    if(node_idx >= num_all_nodes)
    {
        return;
    }

    OutBVHNodeData[node_idx].parent_idx = 0xFFFFFFFFu;
    OutBVHNodeData[node_idx].left_idx = 0xFFFFFFFFu;
    OutBVHNodeData[node_idx].right_idx = 0xFFFFFFFFu;

    if(node_idx > (num_interal_nodes - 1))
    {
        uint prim_idx = InIdxData[node_idx - num_interal_nodes];
        OutBVHNodeData[node_idx].object_idx = prim_idx;   
        OutReorderAABBData[node_idx] = InAABBData[num_interal_nodes + prim_idx];
    }
    else
    {
        OutBVHNodeData[node_idx].object_idx = 0xFFFFFFFFu;

        BVHAABB init_aabb;
        init_aabb.upper = float4(MIN_INT, MIN_INT, MIN_INT, 0.0f);
        init_aabb.lower = float4(MAX_INT, MAX_INT, MAX_INT, 0.0f);  
        OutReorderAABBData[node_idx] = init_aabb;
    }    
}