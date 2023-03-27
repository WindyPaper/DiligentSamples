
#include "Common.csh"

StructuredBuffer<BVHNode> InBVHNodeData;
RWStructuredBuffer<uint> InOutFlag;
RWStructuredBuffer<BVHAABB> OutAABB; //size = num_all_nodes

[numthreads(64, 1, 1)]
void GenerateInternalNodeAABBMain(uint3 id : SV_DispatchThreadID)
{
    uint curr_node_idx = id.x + num_interal_nodes;

    if(curr_node_idx >= num_all_nodes)
    {
        return;
    }

    uint parent = InBVHNodeData[curr_node_idx].parent_idx;
    while(parent != 0xFFFFFFFF) // means idx == 0
    {
        //const int old = atomicCAS(flags + parent, 0, 1);
        uint old;
        InterlockedCompareExchange(InOutFlag[parent], 0u, 1u, old);
        if(old == 0)
        {
            // this is the first thread entered here.
            // wait the other thread from the other child node.
            return;
        }
        //assert(old == 1);
        // here, the flag has already been 1. it means that this
        // thread is the 2nd thread. merge AABB of both childlen.

        const uint lidx = InBVHNodeData[parent].left_idx;
        const uint ridx = InBVHNodeData[parent].right_idx;
        const BVHAABB lbox = OutAABB[lidx];
        const BVHAABB rbox = OutAABB[ridx];
        OutAABB[parent] = merge(lbox, rbox);

        // look the next parent...
        parent = InBVHNodeData[parent].parent_idx;
    }
}