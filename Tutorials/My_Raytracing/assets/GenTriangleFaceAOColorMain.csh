#ifndef TRIANGLE_SUBDIVISION_NUM
#   define TRIANGLE_SUBDIVISION_NUM 16
#endif

cbuffer GenTriangleFaceAOUniformData
{
    int num_triangle;  
}

#include "Common.csh"

StructuredBuffer<GenAOColorData> InTriangleAOPosColorDatas;

RWStructuredBuffer<GenAOColorData> OutTriangleFaceAOColorDatas;

[numthreads(128, 1, 1)]
void GenTriangleFaceAOColorMain(uint3 gid : SV_GroupID, uint3 id : SV_DispatchThreadID, uint local_grp_idx : SV_GroupIndex)
{
    uint triangle_face_idx = id.x;
    if(triangle_face_idx >= num_triangle)
    {
        return;
    }

    uint triangle_subd_pos_start = triangle_face_idx * TRIANGLE_SUBDIVISION_NUM;

    float avg_lum_in_face = 0.0f;

    for(int i = 0; i < TRIANGLE_SUBDIVISION_NUM; ++i)
    {
        avg_lum_in_face += InTriangleAOPosColorDatas[triangle_subd_pos_start + i].lum;
    }

    avg_lum_in_face /= TRIANGLE_SUBDIVISION_NUM;

    OutTriangleFaceAOColorDatas[triangle_face_idx].lum = avg_lum_in_face;    
}