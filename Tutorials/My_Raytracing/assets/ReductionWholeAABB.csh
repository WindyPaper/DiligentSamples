
#include "Common.csh"

// #ifndef PER_GROUP_THREADS_NUM
//     #define PER_GROUP_THREADS_NUM 512
// #endif

cbuffer ReductionUniformData
{
    uint InReductionDataNum;
    uint InAABBIdxOffset;
}

StructuredBuffer<BVHAABB> InWholeAABB;
RWStructuredBuffer<BVHAABB> OutWholeAABB;

groupshared BVHAABB GrpSharedMem[PER_GROUP_THREADS_NUM];

BVHAABB merge(const in BVHAABB lhs, const in BVHAABB rhs)
{
    BVHAABB merged;
    merged.upper.x = max(lhs.upper.x, rhs.upper.x);
    merged.upper.y = max(lhs.upper.y, rhs.upper.y);
    merged.upper.z = max(lhs.upper.z, rhs.upper.z);
    merged.upper.w = 1.0f;
    merged.lower.x = min(lhs.lower.x, rhs.lower.x);
    merged.lower.y = min(lhs.lower.y, rhs.lower.y);
    merged.lower.z = min(lhs.lower.z, rhs.lower.z);
    merged.lower.w = 1.0f;

    return merged;
}

void make_aabb_empty(inout BVHAABB v)
{
    v.upper.x = MIN_INT;
    v.upper.y = MIN_INT;
    v.upper.z = MIN_INT;
    v.upper.w = 0.0;
    v.lower.x = MAX_INT;
    v.lower.y = MAX_INT;
    v.lower.z = MAX_INT;
    v.lower.w = 0.0;
}

[numthreads(PER_GROUP_THREADS_NUM, 1, 1)]
void ReductionWholeAABBMain(uint3 gid : SV_GroupID, uint3 tid : SV_DispatchThreadID, uint local_grp_idx : SV_GroupIndex)
{
    // if(tid.x >= InReductionDataNum)
    // {
    //     return;
    // }   

    uint disp_thread_idx = tid.x;

    BVHAABB empty_aabb;
    make_aabb_empty(empty_aabb);

    if(disp_thread_idx >= InReductionDataNum)
    {
        GrpSharedMem[local_grp_idx] = empty_aabb;    
    }
    else
    {
        GrpSharedMem[local_grp_idx] = InWholeAABB[InAABBIdxOffset + disp_thread_idx]; // store in shared memory
    }

	GroupMemoryBarrierWithGroupSync(); // wait until everything is transfered from device memory to shared memorys

	for (uint s = PER_GROUP_THREADS_NUM / 2; s > 0; s >>= 1)
    {
		if (local_grp_idx < s)
        {
            GrpSharedMem[local_grp_idx] = merge(GrpSharedMem[local_grp_idx], GrpSharedMem[local_grp_idx + s]);
            //GrpSharedMem[local_grp_idx] += GrpSharedMem[local_grp_idx + s];
        }		
		GroupMemoryBarrierWithGroupSync();
	}

	// // Have the first thread write out to the output
	if (local_grp_idx == 0){
		// write out the result for each thread group        
		OutWholeAABB[gid.x] = GrpSharedMem[local_grp_idx];//GrpSharedMem[0];
	}

    // BVHAABB aabb;
    // aabb.upper = float4(MIN_INT, MIN_INT, MIN_INT, 0.0f);
    // aabb.lower = float4(MAX_INT, MAX_INT, MAX_INT, 0.0f);    

    // uint start_idx = prim_idx * 3;
    // for(int i = 0; i < 3; ++i)
    // {
    //     uint curr_vertex_idx = MeshIdx[start_idx + i];
    //     BVHVertex v = MeshVertex[curr_vertex_idx];

    //     aabb.upper.xyz = max(v.pos, aabb.upper.xyz);
    //     aabb.lower.xyz = min(v.pos, aabb.lower.xyz);
    // }

    //OutWholeAABB[num_interal_nodes + prim_idx] = aabb;
    //OutWholeAABB[0]
}