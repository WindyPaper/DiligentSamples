
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
    int padding;    
}