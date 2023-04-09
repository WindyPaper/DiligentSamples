#include "Common.csh"

cbuffer MergeBitonicSortMortonUniformData
{
    uint per_merge_code_num;
    uint pass_idx;
}

StructuredBuffer<uint> InMortonCodeData;
// StructuredBuffer<uint> InIdxData;

RWStructuredBuffer<uint> OutSortMortonCodeData;
RWStructuredBuffer<uint> OutIdxData;

// groupshared uint16_t LocalGroupPositiveBitState[SORT_GROUP_THREADS_NUM];
// groupshared uint16_t LocalGroupInverseBitState[SORT_GROUP_THREADS_NUM];

[numthreads(SORT_GROUP_THREADS_NUM, 1, 1)]
void MergeBitonicSortMortonCodeMain(uint3 id : SV_DispatchThreadID)
{
    // uint global_prim_idx = id.x;    

    // int jump_scale = floor(float(global_prim_idx) / per_merge_code_num);

    // uint start_base_idx = id.x + jump_scale * per_merge_code_num;
    // uint local_offset = global_prim_idx % per_merge_code_num;

    if(id.x >= upper_pow_of_2_primitive_num)
    {
        return;
    }

    uint real_merge_code_num = min(upper_pow_of_2_primitive_num>>1, per_merge_code_num);

    uint mid_idx = real_merge_code_num >> pass_idx;

    // uint pass_idx = 0;
    // uint start_idx = start_base_idx;
    uint offset_scale = id.x / float(mid_idx);
    uint start_idx = id.x + offset_scale * mid_idx;

    // while(mid_idx > 0)
    // {
    uint get_pair_idx = start_idx + mid_idx;
    if(pass_idx == 0)
    {
        //reverse
        get_pair_idx = start_idx + (real_merge_code_num - (start_idx % real_merge_code_num)) + real_merge_code_num - (start_idx % real_merge_code_num);
        get_pair_idx -= 1;        
    }    

    uint ori_min_val = InMortonCodeData[start_idx];
    uint min_val = min(InMortonCodeData[start_idx], InMortonCodeData[get_pair_idx]);
    uint max_val = max(InMortonCodeData[start_idx], InMortonCodeData[get_pair_idx]);

    // AllMemoryBarrierWithGroupSync();        

    OutSortMortonCodeData[start_idx] = min_val;
    OutSortMortonCodeData[get_pair_idx] = max_val;            

    if(min_val != ori_min_val)
    {
        //swap
        uint temp_i = OutIdxData[start_idx];
        OutIdxData[start_idx] = OutIdxData[get_pair_idx];
        OutIdxData[get_pair_idx] = temp_i;
    }
       
    // }

}