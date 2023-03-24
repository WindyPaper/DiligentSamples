
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