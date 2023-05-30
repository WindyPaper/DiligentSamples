#ifndef TRIANGLE_SUBDIVISION_NUM 
#   define TRIANGLE_SUBDIVISION_NUM 16
#endif

#define SQRT_TRIANGLE_SUBDIVISION_NUM sqrt(TRIANGLE_SUBDIVISION_NUM)

// cbuffer GenTriangleAORaysUniformData
// {
//     int num_triangle;  
// }

struct GenSubdivisionPosInTriangle
{
    float3 pos;
};

// StructuredBuffer<uint> MeshIdx;

// RWStructuredBuffer<GenAORayData> OutAORayDatas;
RWStructuredBuffer<GenSubdivisionPosInTriangle> OutSubdivisionPositions;

float3x3 bitToXform (in uint bit )
{
  float s = float ( bit ) - 0.5;
  float3 c1 = float3 ( s, -0.5, 0.5);
  float3 c2 = float3 ( -0.5 , -s, 0.5);
  float3 c3 = float3 (0.0 , 0.0 , 1.0);
  return float3x3 (c1 , c2 , c3);
}

float3x3 keyToXform (in uint key )
{
  float3x3 xf = float3x3(float3(1.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), float3(0.0f, 0.0f, 1.0f)) ;
  for(int i = 0; i < SQRT_TRIANGLE_SUBDIVISION_NUM; ++i)
  {
    xf = mul(bitToXform( key & 1u), xf);
    key = key >> 1u;
  }
  return xf;
}

// float3 berp (in float3 vertex[3] , in float2 lerp_uv)
// {
//   float u = 1.0f - lerp_uv.x - lerp_uv.y;
//   float v = lerp_uv.x;
//   float w = lerp_uv.y;
//   return vertex[0] * u + v * vertex[1] + w * vertex[2];
// }

float3 berp (in float3 v[3] , in float2 u)
{
  return v [0] + u.x * (v [1] - v [0]) + u.y * (v [2] - v [0]) ;
}
// // subdivision routine ( vertex position only )
// void subd (in uint key , in float3 v_in [3] , out float3 v_out[3])
// {
//   float3x3 xf = keyToXform ( key);
//   float2 u1 = (xf * float3(0.0f, 0.0f, 1.0f)).xy;
//   float2 u2 = (xf * float3(1.0f, 0.0f, 1.0f)).xy;
//   float2 u3 = (xf * float3(0.0f, 1.0f, 1.0f)).xy;
//   v_out[0] = berp (v_in , u1);
//   v_out[1] = berp (v_in , u2);
//   v_out[2] = berp (v_in , u3);
// }

float3 get_subd_center(in uint key, in float3 v_in[3])
{
    float3x3 xf = keyToXform ( key);
    float2 u1 = mul(xf, float3 (0.0f, 0.0f, 1.0f)).xy;
    float2 u2 = mul(xf, float3 (0.0f, 1.0f, 1.0f)).xy;
    float2 u3 = mul(xf, float3 (1.0f, 0.0f, 1.0f)).xy;

    float3 v_out0, v_out1, v_out2;
    v_out0 = berp (v_in , u1);
    v_out1 = berp (v_in , u2);
    v_out2 = berp (v_in , u3);

    return (v_out0 + v_out1 + v_out2) / 3.0f;
}

[numthreads(TRIANGLE_SUBDIVISION_NUM, 1, 1)]
void GenTriangleAOPosMain(uint3 gid : SV_GroupID, uint3 id : SV_DispatchThreadID, uint local_grp_idx : SV_GroupIndex)
{
    uint triangle_idx = gid.x;
    //uint triangle_ray_pos_idx_start = triangle_idx * TRIANGLE_SUBDIVISION_NUM;

    uint triangle_pos_id = triangle_idx * TRIANGLE_SUBDIVISION_NUM + local_grp_idx;    

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

    // GenSubdivisionPosInTriangle subd_pos_in_triangle;
    // subd_pos_in_triangle.pos = float3(0.0, 0.0, 0.0);
    // OutSubdivisionPositions[gid.x] = subd_pos_in_triangle;
    // // GroupMemoryBarrierWithGroupSync();

    // // if(local_grp_idx == 0)
    // {    
    float3 in_subd_v[3];
    in_subd_v[0] = pos0;
    in_subd_v[1] = pos1;
    in_subd_v[2] = pos2;
        //for(int i = 0; i < TRIANGLE_SUBDIVISION_NUM; ++i)
        //{
    GenSubdivisionPosInTriangle subd_pos_in_triangle;
    subd_pos_in_triangle.pos = get_subd_center(local_grp_idx, in_subd_v);
    OutSubdivisionPositions[triangle_pos_id] = subd_pos_in_triangle;
        //}
    // }
}