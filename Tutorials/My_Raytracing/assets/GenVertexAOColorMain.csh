#ifndef VERTEX_AO_SAMPLE_NUM
#   define VERTEX_AO_SAMPLE_NUM 256
#endif

// cbuffer GenVertexAORaysUniformData
// {
//     int num_vertex;  
// }

struct GenAOColorData
{
    float lum;
};

StructuredBuffer<GenAORayData> AORayDatas;

RWStructuredBuffer<GenAOColorData> OutAOColorDatas;

Texture2D BakeAOTexture;
SamplerState BakeAOTexture_sampler; // By convention, texture samplers must use the '_sampler' suffix

groupshared uint GroupRayHitTime;

float3 OffsetRayPos(float3 tangent, float3 binormal, float3 vertex_pos)
{
    float3 rays[4];
    rays[0] = tangent;
    rays[1] = -tangent;
    rays[2] = binormal;
    rays[3] = -binormal;

    const float pos_bia = 0.001f;
    const float hit_back_face_dist_threshold = 0.1f;
    
    //trace 4 ray        
    for(int i = 0; i < 4; ++i)
    {        
        float3 ray_dir = rays[i];
        RayData ray;
        ray.dir = ray_dir;
        ray.o = vertex_pos + ray_dir * pos_bia;
        float min_near = MAX_INT;
        uint hit_idx_prim = -1;
        float2 hit_coordinate = 0;
        bool back_face = false;

        RayTrace(ray, min_near, hit_idx_prim, hit_coordinate, back_face);

        if(hit_idx_prim != -1 && back_face == true)
        {
            if(min_near < hit_back_face_dist_threshold)
            {
                return ray_dir * (hit_back_face_dist_threshold);
            }
        }
    }

    return float3(0.0f, 0.0f, 0.0f);
}

[numthreads(VERTEX_AO_SAMPLE_NUM, 1, 1)]
void GenVertexAOColorMain(uint3 gid : SV_GroupID, uint3 id : SV_DispatchThreadID, uint local_grp_idx : SV_GroupIndex)
{
    GroupRayHitTime = 0;
    GroupMemoryBarrierWithGroupSync();

    uint vertex_ray_id = id.x;
    uint vertex_idx = gid.x;
    uint group_ray_idx = local_grp_idx;

    float3 ray_pos = MeshVertex[vertex_idx].pos;
    float3 ray_dir = AORayDatas[VERTEX_AO_SAMPLE_NUM * vertex_idx + group_ray_idx].dir;

    //offset ray_pos
    float3 local_normal = MeshVertex[vertex_idx].normal;    
    float3 tangent = cross(local_normal, float3(0, 0, 1));
	tangent = length(tangent) < 0.1 ? cross(local_normal, float3(0, 1, 0)) : tangent;
	tangent = normalize(tangent);
	float3 binormal = normalize(cross(tangent, local_normal));
    // ray_pos += OffsetRayPos(tangent, binormal, ray_pos);


    const float pos_bia = 0.001f;

    RayData ray;
    ray.dir = ray_dir;
    ray.o = ray_pos + ray_dir * pos_bia;
    float min_near = MAX_INT;
    uint hit_idx_prim = -1;
    float2 hit_coordinate = 0;
    bool back_face = false;

    RayTrace(ray, min_near, hit_idx_prim, hit_coordinate, back_face);

    if(hit_idx_prim != -1) //hit
    {
        uint src_val;
        InterlockedAdd(GroupRayHitTime, 1, src_val);
    }

    GroupMemoryBarrierWithGroupSync();

    if(group_ray_idx == 0)
    {
        float ao_val = 1.0f - float(GroupRayHitTime) / VERTEX_AO_SAMPLE_NUM;
        float4 diff_tex_data = BakeAOTexture.SampleLevel(BakeAOTexture_sampler, MeshVertex[vertex_idx].uv1, 0);
        OutAOColorDatas[vertex_idx].lum = saturate(ao_val);
    }
}