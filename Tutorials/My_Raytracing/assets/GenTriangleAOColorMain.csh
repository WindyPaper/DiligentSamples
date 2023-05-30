#ifndef VERTEX_AO_SAMPLE_NUM
#   define VERTEX_AO_SAMPLE_NUM 256
#endif

// cbuffer GenVertexAORaysUniformData
// {
//     int num_vertex;  
// }

StructuredBuffer<GenAORayData> TriangleAORayDatas;
StructuredBuffer<GenSubdivisionPosInTriangle> TriangleAOPosDatas;

RWStructuredBuffer<GenAOColorData> OutTriangleAOColorDatas;

groupshared float AO_Irradiance[VERTEX_AO_SAMPLE_NUM];

[numthreads(VERTEX_AO_SAMPLE_NUM, 1, 1)]
void GenTriangleAOColorMain(uint3 gid : SV_GroupID, uint3 id : SV_DispatchThreadID, uint local_grp_idx : SV_GroupIndex)
{
    // GroupRayHitTime = 0;
    // GroupMemoryBarrierWithGroupSync();
    AO_Irradiance[local_grp_idx] = 0.0f;
    
    uint subd_triangle_pos_idx = gid.x;
    uint group_ray_idx = local_grp_idx;
    uint triangle_idx = uint(subd_triangle_pos_idx / TRIANGLE_SUBDIVISION_NUM);

    float3 ray_pos = TriangleAOPosDatas[subd_triangle_pos_idx].pos;
    float3 ray_dir = TriangleAORayDatas[VERTEX_AO_SAMPLE_NUM * triangle_idx + group_ray_idx].dir; 

    uint p0_i = MeshIdx[triangle_idx * 3];
    uint p1_i = MeshIdx[triangle_idx * 3 + 1];
    uint p2_i = MeshIdx[triangle_idx * 3 + 2];
    float3 pos0 = MeshVertex[p0_i].pos;
    float3 pos1 = MeshVertex[p1_i].pos;
    float3 pos2 = MeshVertex[p2_i].pos;
    float3 local_normal = normalize(cross(pos1 - pos0, pos2 - pos0));

    const float pos_bia = 0.001f;

    RayData ray;
    ray.dir = ray_dir;
    ray.o = ray_pos + ray_dir * pos_bia;
    float min_near = MAX_INT;
    uint hit_idx_prim = -1;
    float2 hit_coordinate = 0;
    bool back_face = false;

    RayTrace(ray, min_near, hit_idx_prim, hit_coordinate, back_face);

    if(hit_idx_prim == -1) //not hit triangles, hit sky
    {
        AO_Irradiance[local_grp_idx] += 1.0f + 0.0000001f * saturate(dot(ray_dir, local_normal));
    }

    GroupMemoryBarrierWithGroupSync();

    if(group_ray_idx == 0)
    {
        float all_rad = 0.0f;

        for(int i = 0; i < VERTEX_AO_SAMPLE_NUM; ++i)
        {
            all_rad += AO_Irradiance[i];
        }
        all_rad /= VERTEX_AO_SAMPLE_NUM;

        OutTriangleAOColorDatas[subd_triangle_pos_idx].lum =  all_rad;
    }
}