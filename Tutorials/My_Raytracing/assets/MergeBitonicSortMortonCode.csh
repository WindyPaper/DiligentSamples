#include "Common.csh"

cbuffer MergeBitonicSortMortonUniformData
{
    uint per_merge_code_num;
}

StructuredBuffer<uint> InMortonCodeData;
// StructuredBuffer<uint> InIdxData;

RWStructuredBuffer<uint> OutSortMortonCodeData;
RWStructuredBuffer<uint> OutIdxData;

// groupshared uint16_t LocalGroupPositiveBitState[SORT_GROUP_THREADS_NUM];
// groupshared uint16_t LocalGroupInverseBitState[SORT_GROUP_THREADS_NUM];

[numthreads(SORT_GROUP_THREADS_NUM, 1, 1)]
void MergeBitonicSortMortonCodeMain(uint3 id : SV_DispatchThreadID, uint local_grp_idx : SV_GroupIndex, uint3 grp_id : SV_GroupID)
{
    uint global_prim_idx = id.x;
    // uint local_prim_idx = local_grp_idx.x;
    // uint grp_idx = grp_id.x;

    // if(global_prim_idx > end_num_idx)
    // {
    //     return;
    // }

    // uint merge_code_num = SORT_GROUP_THREADS_NUM * 2 * (pass_id + 1);
    // uint start_num_idx = grp_idx * merge_code_num;

    // uint pair_idx = upper_pow_of_2_primitive_num>>1;
    
    // //bitonic sort
    // int pass_idx = 0;
    // while(pair_idx > 0)
    // {
    //     uint get_pair_idx = global_prim_idx + pair_idx;
    //     if(pass_idx == 0)
    //     {
    //         get_pair_idx = upper_pow_of_2_primitive_num - global_prim_idx;
    //     }
    // }

    int jump_scale = floor(float(global_prim_idx) / per_merge_code_num);

    uint start_base_idx = id.x + jump_scale * per_merge_code_num;
    uint local_offset = global_prim_idx % per_merge_code_num;

    uint mid_idx = per_merge_code_num;

    uint pass_idx = 0;
    uint start_idx = start_base_idx;
    while(mid_idx > 0)
    {
        uint get_pair_idx = start_idx + mid_idx;
        uint min_val, max_val;
        uint ori_min_val;
        if(pass_idx == 0)
        {
            //reverse
            get_pair_idx = start_idx + (per_merge_code_num - (start_idx % per_merge_code_num)) + per_merge_code_num - (start_idx % per_merge_code_num);
            get_pair_idx -= 1;
            
            ori_min_val = InMortonCodeData[start_idx];
            min_val = min(InMortonCodeData[start_idx], InMortonCodeData[get_pair_idx]);
            max_val = max(InMortonCodeData[start_idx], InMortonCodeData[get_pair_idx]);
        }
        else
        {
            ori_min_val = OutSortMortonCodeData[start_idx];
            min_val = min(OutSortMortonCodeData[start_idx], OutSortMortonCodeData[get_pair_idx]);
            max_val = max(OutSortMortonCodeData[start_idx], OutSortMortonCodeData[get_pair_idx]);        
        }

        //DeviceMemoryBarrierWithGroupSync();        

        OutSortMortonCodeData[start_idx] = min_val;
        OutSortMortonCodeData[get_pair_idx] = max_val;

        // if(min_val == 0)
        // {
        //     OutSortMortonCodeData[start_idx] = global_prim_idx;
        //     OutSortMortonCodeData[get_pair_idx] = get_pair_idx;
        // }

        if(min_val != ori_min_val)
        {
            //swap
            uint temp_i = OutIdxData[start_idx];
            OutIdxData[start_idx] = OutIdxData[get_pair_idx];
            OutIdxData[get_pair_idx] = temp_i;
        }

        ++pass_idx;        
        mid_idx = mid_idx >> 1;
        //uint offset = (id.x % mid_idx);
        uint offset_scale = local_offset / float(mid_idx);
        start_idx = start_base_idx + offset_scale * mid_idx;

        DeviceMemoryBarrierWithGroupSync();
    }

}