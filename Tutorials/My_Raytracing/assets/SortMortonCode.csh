#include "Common.csh"

cbuffer SortMortonUniformData
{
    uint pass_id;
    uint sort_num;
}

StructuredBuffer<uint> InMortonCodeData;
StructuredBuffer<uint> InIdxData;

RWStructuredBuffer<uint> OutSortMortonCodeData;
RWStructuredBuffer<uint> OutIdxData;

groupshared uint16_t LocalGroupPositiveBitState[SORT_GROUP_THREADS_NUM];
groupshared uint16_t LocalGroupInverseBitState[SORT_GROUP_THREADS_NUM];

[numthreads(SORT_GROUP_THREADS_NUM, 1, 1)]
void SortMortonCodeMain(uint3 id : SV_DispatchThreadID, uint local_grp_idx : SV_GroupIndex, uint3 grp_id : SV_GroupID)
{
    uint global_prim_idx = id.x;
    uint local_prim_idx = local_grp_idx.x;

    // if(local_prim_idx >= sort_num)
    // {
    //     return;
    // }

    // LocalGroupSortData[local_grp_idx] = InMortonCodeData[local_prim_idx];
    // GroupMemoryBarrierWithGroupSync();

    uint clamp_global_prim_idx = min(upper_pow_of_2_primitive_num - 1, global_prim_idx);
    uint16_t bit = (InMortonCodeData[clamp_global_prim_idx] >> pass_id) & 1;
    uint16_t invert_e_bit = ~bit & 1;

    LocalGroupInverseBitState[local_prim_idx] = invert_e_bit;
    LocalGroupPositiveBitState[local_prim_idx] = LocalGroupInverseBitState[local_prim_idx];
    GroupMemoryBarrierWithGroupSync();

    // if(local_prim_idx >= sort_num)
    // {
    //     return;
    // }

    //scan up-sweep
    uint16_t in_bit_i = 0, out_bit_i = 1;
    for (int d = 0; d <= log2(sort_num) - 1; ++d)
    {
        if ((local_prim_idx % (1 << d + 1)) == 0)
        {
            in_bit_i = d & 1;
            // vector<int>& in_bit = LocalGroupPositiveBitState;					
            LocalGroupPositiveBitState[local_prim_idx + (1 << (d + 1)) - 1] = \
                LocalGroupPositiveBitState[local_prim_idx + (1 << d) - 1] + LocalGroupPositiveBitState[local_prim_idx + (1 << (d + 1)) - 1];
        }

        GroupMemoryBarrierWithGroupSync();
    }
    //GroupMemoryBarrierWithGroupSync();

    //scan down-sweep
    LocalGroupPositiveBitState[sort_num - 1] = 0;
    GroupMemoryBarrierWithGroupSync();

    for (int d = log2(sort_num) - 1; d >= 0; --d)
    {
        if ((local_prim_idx % (1 << d + 1)) == 0)
        {
            uint pow_2_d = (1 << d);
            uint pow_2_d_plus_1 = (1 << d + 1);
            uint t = LocalGroupPositiveBitState[local_prim_idx + pow_2_d - 1];
            LocalGroupPositiveBitState[local_prim_idx + pow_2_d - 1] = LocalGroupPositiveBitState[local_prim_idx + pow_2_d_plus_1 - 1];
            LocalGroupPositiveBitState[local_prim_idx + pow_2_d_plus_1 - 1] = t + LocalGroupPositiveBitState[local_prim_idx + pow_2_d_plus_1 - 1];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    //calculate index value
    // vector<int>& out_bit_result = LocalGroupPositiveBitState;
    //int e_value = LocalGroupInverseBitState[local_prim_idx];
    uint16_t bit_value = bit;
    uint total_falses = LocalGroupInverseBitState[sort_num - 1] + LocalGroupPositiveBitState[sort_num - 1];
    uint t_value = local_prim_idx - LocalGroupPositiveBitState[local_prim_idx] + total_falses;
    uint sort_index = bit_value ? t_value : LocalGroupPositiveBitState[local_prim_idx];

    if(global_prim_idx < upper_pow_of_2_primitive_num)
    {
        uint offset_idx = grp_id.x * SORT_GROUP_THREADS_NUM;
        OutSortMortonCodeData[offset_idx + sort_index] = InMortonCodeData[global_prim_idx];
        OutIdxData[offset_idx + sort_index] = InIdxData[global_prim_idx];
    }    
}