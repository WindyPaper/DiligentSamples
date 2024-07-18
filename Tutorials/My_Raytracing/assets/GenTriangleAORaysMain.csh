#ifndef TRIANGLE_AO_SAMPLE_NUM
#   define TRIANGLE_AO_SAMPLE_NUM 256
#endif

[numthreads(TRIANGLE_AO_SAMPLE_NUM, 1, 1)]
void GenTriangleAORaysMain(uint3 gid : SV_GroupID, uint3 id : SV_DispatchThreadID, uint local_grp_idx : SV_GroupIndex)
{
    uint triangle_idx = gid.x;

    uint triangle_ray_id = triangle_idx * TRIANGLE_AO_SAMPLE_NUM + local_grp_idx.x;    

    // if(vertex_ray_id > (num_vertex * TRIANGLE_AO_SAMPLE_NUM - 1))
    // {
    //     return;
    // }

    uint p0_i = MeshIdx[triangle_idx * 3];
    uint p1_i = MeshIdx[triangle_idx * 3 + 1];
    uint p2_i = MeshIdx[triangle_idx * 3 + 2];

    float3 pos0 = MeshVertex[p0_i].pos;
    float3 pos1 = MeshVertex[p1_i].pos;
    float3 pos2 = MeshVertex[p2_i].pos;

    float2 sample_uv = SamplePoint(triangle_ray_id, triangle_idx, local_grp_idx.x);
    float2 concentric_map_point = ConcentricSampleDisk(sample_uv);
	float3 ray_in_tangent_space = LiftPoint2DToHemisphere(concentric_map_point);

    //float3 local_normal = MeshVertex[vertex_idx].normal;
    float3 local_normal = normalize(cross(pos1 - pos0, pos2 - pos0));
    // float3 local_position = MeshVertex[vertex_idx].pos;

    float3 tangent = cross(local_normal, float3(0, 0, 1));
	tangent = length(tangent) < 0.1 ? cross(local_normal, float3(0, 1, 0)) : tangent;
	tangent = normalize(tangent);
	float3 binormal = normalize(cross(tangent, local_normal));

    float3 ray_in_obj_space = normalize(tangent * ray_in_tangent_space.x + binormal * ray_in_tangent_space.y + local_normal * ray_in_tangent_space.z);

    GenAORayData out_ray;
    out_ray.dir = float4(ray_in_obj_space, 0.0f);

    OutAORayDatas[triangle_ray_id] = out_ray;    
}