
#include "Common.csh"

struct BVHDebugData
{
    int unorder_num_idx;
};

StructuredBuffer<uint> SortMortonCodeNums;
RWStructuredBuffer<BVHDebugData> OutDebugData;


[numthreads(64, 1, 1)]
void DebugBVHDataMain(uint3 id : SV_DispatchThreadID)
{
    uint prim_idx = id.x;

    if(prim_idx >= (num_objects - 1))
    {
        return;
    }

    if(SortMortonCodeNums[prim_idx] > SortMortonCodeNums[prim_idx + 1])
    {
        OutDebugData[0].unorder_num_idx = prim_idx;
    }
}