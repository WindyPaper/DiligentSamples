
#include "Common.csh"

// struct BVHVertex
// {
//     float3 pos;
//     float2 uv;
// };

StructuredBuffer<uint> InSortMortonCode;
RWStructuredBuffer<BVHNode> OutBVHNodeData;

int common_upper_bits(uint a, uint additional_a, uint b, uint additional_b)
{
    // uint loca = firstbithigh(a);
    // uint locb = firstbithigh(b);
    // return min(loca, locb);
    uint high_0_num = 31 - firstbithigh(a ^ b);

    uint low_0_num = 0;
    if(high_0_num == 32)
    {
        low_0_num = 31 - firstbithigh(additional_a ^ additional_b);
    }

    return high_0_num + low_0_num;
}

uint2 determine_range(const uint num_leaves, uint idx)
{
    if(idx == 0)
    {
        return uint2(0, num_leaves-1);
    }

    // determine direction of the range
    const uint self_idx = idx;
    const uint self_code = InSortMortonCode[idx];    
    const int L_delta = common_upper_bits(self_code, self_idx, InSortMortonCode[idx-1], idx - 1);
    const int R_delta = common_upper_bits(self_code, self_idx, InSortMortonCode[idx+1], idx + 1);
    const int d = (R_delta > L_delta) ? 1 : -1;

    // Compute upper bound for the length of the range

    const int delta_min = min(L_delta, R_delta);
    int l_max = 2;
    int delta = -1;
    int i_tmp = idx + d * l_max;
    if(0 <= i_tmp && i_tmp < num_leaves)
    {
        delta = common_upper_bits(self_code, self_idx, InSortMortonCode[i_tmp], i_tmp);
    }
    while(delta > delta_min)
    {
        l_max <<= 1;
        i_tmp = idx + d * l_max;
        delta = -1;
        if(0 <= i_tmp && i_tmp < num_leaves)
        {
            delta = common_upper_bits(self_code, self_idx, InSortMortonCode[i_tmp], i_tmp);
        }
    }

    // Find the other end by binary search
    int l = 0;
    int t = l_max >> 1;
    while(t > 0)
    {
        i_tmp = idx + (l + t) * d;
        delta = -1;
        if(0 <= i_tmp && i_tmp < num_leaves)
        {
            delta = common_upper_bits(self_code, self_idx, InSortMortonCode[i_tmp], i_tmp);
        }
        if(delta > delta_min)
        {
            l += t;
        }
        t >>= 1;
    }
    uint jdx = idx + l * d;
    if(d < 0)
    {
        //swap(idx, jdx); // make it sure that idx < jdx
        uint tt = idx;
        idx = jdx;
        jdx = tt;
    }
    return uint2(idx, jdx);
}

uint find_split(const uint num_leaves, const uint first, const uint last)
{
    const uint first_code = InSortMortonCode[first];
    const uint last_code  = InSortMortonCode[last];
    // if (first_code == last_code)
    // {
    //     return (first + last) >> 1;
    // }
    const int delta_node = common_upper_bits(first_code, first, last_code, last);

    // binary search...
    int split  = first;
    int stride = last - first;
    while(stride > 1)
    {
        stride = (stride + 1) >> 1;
        const int middle = split + stride;
        if (middle < last)
        {
            const int delta = common_upper_bits(first_code, first, InSortMortonCode[middle], middle);
            if (delta > delta_node)
            {
                split = middle;
            }
        }
    }

    return split;
}

[numthreads(64, 1, 1)]
void ConstructInternalBVHNodeMain(uint3 id : SV_DispatchThreadID)
{
    uint node_idx = id.x;

    if(node_idx >= num_interal_nodes)
    {
        return;
    }

    const uint2 ij  = determine_range(num_objects, node_idx);
    const int gamma = find_split(num_objects, ij.x, ij.y);

    OutBVHNodeData[node_idx].left_idx  = gamma;
    OutBVHNodeData[node_idx].right_idx = gamma + 1;
    if(min(ij.x, ij.y) == gamma)
    {
        OutBVHNodeData[node_idx].left_idx += num_objects - 1;
    }
    if(max(ij.x, ij.y) == gamma + 1)
    {
        OutBVHNodeData[node_idx].right_idx += num_objects - 1;
    }
    OutBVHNodeData[OutBVHNodeData[node_idx].left_idx].parent_idx  = node_idx;
    OutBVHNodeData[OutBVHNodeData[node_idx].right_idx].parent_idx = node_idx;
}