
#include "Common.csh"

// struct BVHVertex
// {
//     float3 pos;
//     float2 uv;
// };


StructuredBuffer<uint> InIdxData;
RWStructuredBuffer<BVHNode> OutBVHNodeData;


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
        OutBVHNodeData[node_idx].object_idx = InIdxData[node_idx - num_interal_nodes];    
    }
    else
    {
        OutBVHNodeData[node_idx].object_idx = 0xFFFFFFFFu;
    }    
}