#define uint16_t min16uint

struct BVHVertex
{
    float3 pos;
    float2 uv;
};

struct BVHMeshPrimData
{
    int tex_idx;
};

struct BVHAABB
{
    float4 upper;
    float4 lower;
};

cbuffer BVHGlobalData
{
    int num_objects;
    int num_interal_nodes;
    int num_all_nodes;
    int upper_pow_of_2_primitive_num;    
}

static const int MAX_INT = 2147483647;
static const int MIN_INT = -2147483648;

struct BVHNode
{
    uint parent_idx;
    uint left_idx;
    uint right_idx;
    uint object_idx;    
};


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