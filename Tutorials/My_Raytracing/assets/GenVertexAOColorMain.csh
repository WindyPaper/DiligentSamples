#ifndef VERTEX_AO_SAMPLE_NUM
#   define VERTEX_AO_SAMPLE_NUM 256
#endif

cbuffer GenVertexAORaysUniformData
{
    int num_vertex;  
}

struct GenAOColorData
{
    float lum;
};


StructuredBuffer<GenAORayData> AORayDatas;

RWStructuredBuffer<GenAOColorData> OutAOColorDatas;

groupshared uint GroupRayHitTime;

[numthreads(VERTEX_AO_SAMPLE_NUM, 1, 1)]
void GenVertexAOColorMain(uint3 gid : SV_GroupID, uint3 id : SV_DispatchThreadID, uint local_grp_idx : SV_GroupIndex)
{
    uint vertex_ray_id = id.x;
    uint vertex_idx = gid.x;
    uint group_ray_idx = local_grp_idx.x;

    float3 ray_pos = MeshVertex[vertex_idx].pos;
    float3 ray_dir = AORayDatas[VERTEX_AO_SAMPLE_NUM * vertex_idx + group_ray_idx];

    RayData ray;
    ray.dir = ray_dir;
    ray.o = ray_pos;
    float min_near = MAX_INT;
    uint hit_idx_prim = -1;
    float2 hit_coordinate = 0;

    RayTrace(ray, min_near, hit_idx_prim, hit_coordinate);

    if(hit_idx_prim != -1) //hit
    {
        uint src_val;
        InterlockedAdd(GroupRayHitTime, 1, src_val);
    }

    GroupMemoryBarrierWithGroupSync();

    OutAOColorDatas[vertex_idx].lum = float(GroupRayHitTime) / VERTEX_AO_SAMPLE_NUM;
}