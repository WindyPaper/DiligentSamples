#ifndef BAKE_MESH_TEX_XY
    #define BAKE_MESH_TEX_XY 256
#endif

#ifndef BAKE_MESH_TEX_Z
    #define BAKE_MESH_TEX_Z 32
#endif

cbuffer TraceBakeMeshData
{
    float4 BakeVerticalNorDir;
}

RWTexture3D<float4> Out3DTex;

// Rotation with angle (in radians) and axis
float3x3 AngleAxis3x3(float angle, float3 axis)
{
    float c, s;
    sincos(angle, s, c);

    float t = 1 - c;
    float x = axis.x;
    float y = axis.y;
    float z = axis.z;

    return float3x3(
        t * x * x + c,      t * x * y - s * z,  t * x * z + s * y,
        t * x * y + s * z,  t * y * y + c,      t * y * z - s * x,
        t * x * z - s * y,  t * y * z + s * x,  t * z * z + c
    );
}

//[numthreads(BAKE_MESH_TEX_XY, BAKE_MESH_TEX_XY, 1)]
[numthreads(16, 16, 1)]
void TraceBakeMesh3DTexMain(uint3 gid : SV_GroupID, uint3 id : SV_DispatchThreadID)
{
    if(id.x > BAKE_MESH_TEX_XY || id.y > BAKE_MESH_TEX_XY || id.z > BAKE_MESH_TEX_Z)
    {
        return;
    }

    uint3 out_3dtex_idx = id;
    uint layer_idx = id.z;

    float rotate_rad_interp = float(layer_idx) / (BAKE_MESH_TEX_Z - 1);
    float rotate_rad = lerp(0.0f, 3.14f, rotate_rad_interp);

    float3 y_up = float3(0.0f, 1.0f, 0.0f);
    float3x3 rotate_mat = AngleAxis3x3(-rotate_rad, y_up); //neg 
    float3 curr_layer_bake_dir = mul(rotate_mat, BakeVerticalNorDir.xyz);

    BVHAABB MeshAABB = BVHNodeAABB[0];
    float bake_plane_y = MeshAABB.upper.y;

    float bbx_x = MeshAABB.upper.x - MeshAABB.lower.x;
    float bbx_z = MeshAABB.upper.z - MeshAABB.lower.z;

    float per_texel_w = bbx_x / BAKE_MESH_TEX_XY;
    float per_texel_h = bbx_z / BAKE_MESH_TEX_XY;

    float2 ray_start_xz = MeshAABB.lower.xz;
    float2 ray_offset_xz = ray_start_xz + float2((id.x + 0.5f) * per_texel_w, (id.y + 0.5f) * per_texel_h);

    //offset avoid self-intersection
    float3 ray_origin = float3(ray_offset_xz.x, bake_plane_y, ray_offset_xz.y) - curr_layer_bake_dir * 0.01f;

    RayData ray;
    ray.dir = curr_layer_bake_dir;
    ray.o = ray_origin;

    float min_near = MAX_INT;
    uint hit_idx_prim = -1;
    float2 hit_coordinate = 0;
    bool back_face = false;

    RayTrace(ray, min_near, hit_idx_prim, hit_coordinate, back_face);

    if(hit_idx_prim != -1) //Main ray pos start hit
    {
        uint v_idx0 = MeshIdx[hit_idx_prim * 3]; 
        uint v_idx1 = MeshIdx[hit_idx_prim * 3 + 1]; 
        uint v_idx2 = MeshIdx[hit_idx_prim * 3 + 2]; 

        BVHVertex v0 = MeshVertex[v_idx0];
        BVHVertex v1 = MeshVertex[v_idx1];
        BVHVertex v2 = MeshVertex[v_idx2];

        float u = 1.0f - hit_coordinate.x - hit_coordinate.y;
        float v = hit_coordinate.x;
        float w = hit_coordinate.y;
        //float2 out_uv = v1.uv * hit_coordinate.x + v2.uv * hit_coordinate.y + (1.0f - hit_coordinate.x - hit_coordinate.y) * v0.uv;
        float2 out_uv = v0.uv * u + v1.uv * v + v2.uv * w;
        float3 out_normal = v0.normal * u + v1.normal * v + v2.normal * w;       

        float hit_depth = min_near * 0.1f;
        Out3DTex[out_3dtex_idx] = float4(out_normal.xyz, 1.0f / (1.0f + hit_depth));
        //Out3DTex[out_3dtex_idx] = float4(out_normal.xyz, min_near);
    }
    else
    {
        Out3DTex[out_3dtex_idx] = float4(0.0f, 1.0f, 0.0f, 0.0f);

        //surounding 8 neighor element        
        float neighor_hit_min = MAX_INT;
        for(int j = 0; j < 3; ++j)
        {
            for(int i = 0; i < 3; ++i)
            {
                if((i != 1) || (j != 1))
                {
                    float tex_ray_offset_x = -(i - 1) * bbx_x;
                    float tex_ray_offset_z = (j - 1) * bbx_z;

                    hit_idx_prim = -1;
                    min_near = MAX_INT;
                    ray.o = ray_origin - float3(tex_ray_offset_x, 0.0f, tex_ray_offset_z);
                    RayTrace(ray, min_near, hit_idx_prim, hit_coordinate, back_face);
                    if(hit_idx_prim != -1)
                    {                        
                        if(min_near < neighor_hit_min)
                        {
                            neighor_hit_min = min_near;

                            uint v_idx0 = MeshIdx[hit_idx_prim * 3]; 
                            uint v_idx1 = MeshIdx[hit_idx_prim * 3 + 1]; 
                            uint v_idx2 = MeshIdx[hit_idx_prim * 3 + 2]; 

                            BVHVertex v0 = MeshVertex[v_idx0];
                            BVHVertex v1 = MeshVertex[v_idx1];
                            BVHVertex v2 = MeshVertex[v_idx2];

                            float u = 1.0f - hit_coordinate.x - hit_coordinate.y;
                            float v = hit_coordinate.x;
                            float w = hit_coordinate.y;
                            //float2 out_uv = v1.uv * hit_coordinate.x + v2.uv * hit_coordinate.y + (1.0f - hit_coordinate.x - hit_coordinate.y) * v0.uv;
                            //float2 out_uv = v0.uv * u + v1.uv * v + v2.uv * w;
                            float3 out_normal = v0.normal * u + v1.normal * v + v2.normal * w;       

                            float hit_depth = neighor_hit_min * 0.1f;
                            Out3DTex[out_3dtex_idx] = float4(out_normal.xyz, 1.0f / (1.0f + hit_depth));                            
                        }
                    }
                }
            }
        }        
    }
}
