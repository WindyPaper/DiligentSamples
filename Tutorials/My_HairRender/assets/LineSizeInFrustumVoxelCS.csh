//Accumulate line size in frustum voxel

#include "CommonCS.csh"

#define VOXEL_SLICE_NUM 24

struct HairVertexData
{
    float3 Pos;
    int Misc;
};
StructuredBuffer<HairVertexData> VerticesDatas;
StructuredBuffer<uint> IdxData;

Texture2D<float4> DownSampleDepthMap;

RWByteAddressBuffer OutLineAccumulateBuffer;

void AddAccumulateBuffer(int w_x, int w_y, float max_z, float voxel_z_offset, inout int curr_select_tile_ids[3], inout int curr_replace_tile_id)
{       
    int tile_x = int((w_x / 16.0f));
    int tile_y = int((w_y / 16.0f));  
    int tile_id = tile_y * DownSampleDepthSize.x + tile_x;
    if(tile_id != curr_select_tile_ids[0] && tile_id != curr_select_tile_ids[1] && tile_id != curr_select_tile_ids[2])
    {
        float OcclusionDepth = DownSampleDepthMap.Load(int3(tile_x, tile_y, 0)).x;
        if(OcclusionDepth > max_z)
        {
            uint SrcVal;
            uint AccumuBufIdx = (tile_y * DownSampleDepthSize.x + tile_x) * VOXEL_SLICE_NUM + voxel_z_offset;
            //InterlockedAdd(OutLineAccumulateBuffer[AccumuBufIdx], 1, SrcVal);
            OutLineAccumulateBuffer.InterlockedAdd(AccumuBufIdx * 4, 1, SrcVal);

            // curr_select_tile_id = tile_id;

            curr_select_tile_ids[curr_replace_tile_id] = tile_id;
            curr_replace_tile_id = (curr_replace_tile_id + 1) % 3;
        }
    }
}

[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID, uint group_idx : SV_GroupIndex)
{
    uint LineIdx0 = id.x;
    uint VertexIdx0 = IdxData[LineIdx0] & 0x0FFFFFFF;

    HairVertexData V0 = VerticesDatas[VertexIdx0];
    if(isnan(V0.Pos.x))
    {
        return;
    }

    HairVertexData V1 = VerticesDatas[VertexIdx0 + 1];

    float4 VNDC0 = mul(float4(V0.Pos, 1.0f), ViewProj);
    VNDC0.xyz /= VNDC0.w;
    VNDC0.xy = VNDC0.xy * 0.5f + float2(0.5f, 0.5f);
    VNDC0.xy = saturate(VNDC0.xy);
    float4 VNDC1 = mul(float4(V1.Pos, 1.0f), ViewProj);
    VNDC1.xyz /= VNDC1.w;
    VNDC1.xy = VNDC1.xy * 0.5f + float2(0.5f, 0.5f);
    VNDC1.xy = saturate(VNDC1.xy);
    if((abs(VNDC1.x - VNDC0.x) < 0.001f) && (abs(VNDC1.y - VNDC0.y) < 0.001f)) //in same pos
    {
        return;
    }

    float3 LineBBoxMin = float3(min(VNDC0.x, VNDC1.x), min(VNDC0.y, VNDC1.y), min(VNDC0.z, VNDC1.z));
    float3 LineBBoxMax = float3(max(VNDC0.x, VNDC1.x), max(VNDC0.y, VNDC1.y), max(VNDC0.z, VNDC1.z));

    bool IsOutScreen = (LineBBoxMax.x < 0.0f || LineBBoxMax.y < 0.0f || LineBBoxMin.x > 1.0f || LineBBoxMin.y > 1.0f);
    if(IsOutScreen)
    {
        return;
    }

    //voxel z offset
    float LineDistRadio = max(dot(normalize(V0.Pos - HairBBoxMin.xyz), normalize(HairBBoxSize.xyz)), dot(normalize(V1.Pos - HairBBoxMin.xyz), normalize(HairBBoxSize.xyz)));
    uint VoxelZOffset = saturate(LineDistRadio) * (VOXEL_SLICE_NUM - 1);

    float2 StartPixelCoord = VNDC0.xy * ScreenSize.xy;    
    float2 EndPixelCoord = VNDC1.xy * ScreenSize.xy;

    float abs_y = abs(EndPixelCoord.y - StartPixelCoord.y);
    float abs_x = abs(EndPixelCoord.x - StartPixelCoord.x);
    bool steep = abs_y > abs_x;

    if(steep)
    {
        swap(StartPixelCoord.x, StartPixelCoord.y);
        swap(EndPixelCoord.x, EndPixelCoord.y);        
    }
    if(StartPixelCoord.x > EndPixelCoord.x)
    {
        swap(StartPixelCoord.x, EndPixelCoord.x);
        swap(StartPixelCoord.y, EndPixelCoord.y);
    }

    //compute the slope
    float dx = EndPixelCoord.x - StartPixelCoord.x;
    float dy = EndPixelCoord.y - StartPixelCoord.y;

    float gradient = 1.0f;
    if(dx != 0.0f)                
    {
        gradient = dy / dx;
    }

    int s_x = StartPixelCoord.x;
    int e_x = EndPixelCoord.x;
    float intersect_y = StartPixelCoord.y;

    int curr_select_tile_id0 = -1;
    int curr_select_tile_id1 = -1;
    int last_three_tile_id[3];
    last_three_tile_id[0] = -1;
    last_three_tile_id[1] = -1;
    last_three_tile_id[2] = -1;
    int curr_replace_tile_id = 0;
    if(steep)
    {
        for(int i = s_x; i <= e_x; ++i)
        {
            int w_x = floor(intersect_y);
            int w_y = i;

            float bright = 1.0f - frac(intersect_y);
            if(w_x > -1 && w_y < ScreenSize.y)
            {                
                if(bright > 0.0f)
                {
                    AddAccumulateBuffer(w_x, w_y, LineBBoxMax.z, VoxelZOffset, last_three_tile_id, curr_replace_tile_id);
                }
            }
            //OutputTexture[int2(w_x, w_y)] = float4(bright, bright, bright, 1.0f);
            
            w_x = w_x + 1;
            if(w_x > -1 && w_y < ScreenSize.y)
            {                
                bright = frac(intersect_y);
                //OutputTexture[int2(w_x, w_y)] = float4(bright, bright, bright, 1.0f);
                if(bright > 0.0f)
                {
                    AddAccumulateBuffer(w_x, w_y, LineBBoxMax.z, VoxelZOffset, last_three_tile_id, curr_replace_tile_id);
                }
            }

            intersect_y += gradient;
        }
    }
    else
    {
        for(int i = s_x; i <= e_x; ++i)
        {
            int w_x = i;
            int w_y = floor(intersect_y);
            float bright = 1.0f - frac(intersect_y);
            if(w_x > -1 && w_y < ScreenSize.y)
            {                
                if(bright > 0.0f)
                {
                    AddAccumulateBuffer(w_x, w_y, LineBBoxMax.z, VoxelZOffset, last_three_tile_id, curr_replace_tile_id);
                }
            }
            //OutputTexture[int2(w_x, w_y)] = float4(bright, bright, bright, 1.0f);

            w_y = w_y + 1;
            if(w_x > -1 && w_y < ScreenSize.y)
            {
                bright = frac(intersect_y);
                if(bright > 0.0f)
                {
                    AddAccumulateBuffer(w_x, w_y, LineBBoxMax.z, VoxelZOffset, last_three_tile_id, curr_replace_tile_id);
                }
            }
            //OutputTexture[int2(w_x, w_y)] = float4(bright, bright, bright, 1.0f);

            intersect_y += gradient;
        }
    }
}
