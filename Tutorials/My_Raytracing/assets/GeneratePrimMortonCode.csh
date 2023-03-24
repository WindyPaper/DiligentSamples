
#include "Common.csh"

// const int MAX_INT = 2147483647;
// const int MIN_INT = -2147483648;

StructuredBuffer<BVHAABB> whole_aabb;

// StructuredBuffer<BVHAABB> inAABBs; //size = num_all_nodes
StructuredBuffer<float3> inPrimCentroid;

RWStructuredBuffer<uint> outPrimMortonCode;

uint expand_bits(uint v)
{
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

// Calculates a 30-bit Morton code for the
// given 3D point located within the unit cube [0,1].
uint morton_code(float3 xyz)
{
    const float resolution = 1024.0f;

    xyz.x = min(max(xyz.x * resolution, 0.0f), resolution - 1.0f);
    xyz.y = min(max(xyz.y * resolution, 0.0f), resolution - 1.0f);
    xyz.z = min(max(xyz.z * resolution, 0.0f), resolution - 1.0f);
    const uint xx = expand_bits(xyz.x);
    const uint yy = expand_bits(xyz.y);
    const uint zz = expand_bits(xyz.z);
    return xx * 4 + yy * 2 + zz;
}

float3 get_centroid(const in BVHAABB box)
{
    float3 c;
    c.x = (box.upper.x + box.lower.x) * 0.5;
    c.y = (box.upper.y + box.lower.y) * 0.5;
    c.z = (box.upper.z + box.lower.z) * 0.5;
    return c;
}

int create_morton_code(float3 p)
{
    // float3 p = get_centroid(aabb);

    p.x -= whole_aabb[0].lower.x;
    p.y -= whole_aabb[0].lower.y;
    p.z -= whole_aabb[0].lower.z;
    p.x /= (whole_aabb[0].upper.x - whole_aabb[0].lower.x + 1.0f);
    p.y /= (whole_aabb[0].upper.y - whole_aabb[0].lower.y + 1.0f);
    p.z /= (whole_aabb[0].upper.z - whole_aabb[0].lower.z + 1.0f);
    return morton_code(p);
}

[numthreads(64, 1, 1)]
void GeneratePrimMortonCodeMain(uint3 id : SV_DispatchThreadID)
{
    uint prim_idx = id.x;

    if(prim_idx >= num_objects)
    {
        if(prim_idx < upper_pow_of_2_primitive_num)
        {
            outPrimMortonCode[prim_idx] = MAX_INT;
        }        
        return;
    }

    //const BVHAABB aabb = inAABBs[num_interal_nodes + prim_idx];
    const float3 p = inPrimCentroid[prim_idx];
    
    uint prim_morton_code = create_morton_code(p);

    outPrimMortonCode[prim_idx] = prim_morton_code;
}