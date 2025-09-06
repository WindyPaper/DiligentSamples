//Get hair line visibility 

#include "CommonCS.csh"

#define VOXEL_SLICE_NUM 24

struct HairVertexData
{
    float3 Pos;
    int Misc;
};
StructuredBuffer<HairVertexData> VerticesDatas;
StructuredBuffer<uint> IdxData;

ByteAddressBuffer LineOffsetBuffer;

Texture2D<float4> DownSampleDepthMap;

RWByteAddressBuffer OutLineAccumulateBuffer;
RWStructuredBuffer<uint> OutLineVisibilityBuffer;
RWStructuredBuffer<uint> OutLineRenderQueueBuffer;

groupshared uint VisibilityBits[2];

void AddToVisibilityBuffer(uint line_idx, uint grp_idx, float px, float py, float max_z, float voxel_z_offset, inout int curr_select_tile_id)
{
    int DownSampleX = int((px / 16.0f));
    int DownSampleY = int((py / 16.0f));
    int TileID = DownSampleY * DownSampleDepthSize.x + DownSampleX;
    if(TileID != curr_select_tile_id)
    {
        float OcclusionDepth = DownSampleDepthMap.Load(int3(DownSampleX, DownSampleY, 0)).x;
        if(OcclusionDepth > max_z)
        {
            uint CurrLineIdxInOffsetBuf;
            uint AccumuBufIdx = TileID * VOXEL_SLICE_NUM + voxel_z_offset;
            //InterlockedAdd(OutLineAccumulateBuffer[AccumuBufIdx], 1, CurrLineIdxInOffsetBuf);
            OutLineAccumulateBuffer.InterlockedAdd(AccumuBufIdx * 4, 1, CurrLineIdxInOffsetBuf);

            //get offset addr
            uint OffsetBufAddr = LineOffsetBuffer.Load(AccumuBufIdx * 4);
            OutLineRenderQueueBuffer[OffsetBufAddr + CurrLineIdxInOffsetBuf] =  line_idx;

            //add visibility bit flag
            uint BitsIdx = grp_idx >> 5;
            uint BitsValue = grp_idx & 31;
            InterlockedOr(VisibilityBits[BitsIdx], 1u << BitsValue);

            curr_select_tile_id = TileID;
        }
    }
}

[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID, uint group_idx : SV_GroupIndex)
{
    if(group_idx < 2)
    {
        VisibilityBits[group_idx] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint LineIdx0 = id.x;
    uint VertexIdx0 = IdxData[LineIdx0] & 0x0FFFFFFF;

    HairVertexData V0 = VerticesDatas[VertexIdx0];
    if(!isnan(V0.Pos.x))
    {
        HairVertexData V1 = VerticesDatas[VertexIdx0 + 1];

        float4 VNDC0 = mul(float4(V0.Pos, 1.0f), ViewProj);
        VNDC0.xyz /= VNDC0.w;
        VNDC0.xy = VNDC0.xy * 0.5f + float2(0.5f, 0.5f);
        VNDC0.xy = saturate(VNDC0.xy);
        float4 VNDC1 = mul(float4(V1.Pos, 1.0f), ViewProj);
        VNDC1.xyz /= VNDC1.w;
        VNDC1.xy = VNDC1.xy * 0.5f + float2(0.5f, 0.5f);
        VNDC1.xy = saturate(VNDC1.xy);
        if((abs(VNDC1.x - VNDC0.x) > 0.001f) && (abs(VNDC1.y - VNDC0.y) > 0.001f)) //not in same pos
        {            
            float3 LineBBoxMin = float3(min(VNDC0.x, VNDC1.x), min(VNDC0.y, VNDC1.y), min(VNDC0.z, VNDC1.z));
            float3 LineBBoxMax = float3(max(VNDC0.x, VNDC1.x), max(VNDC0.y, VNDC1.y), max(VNDC0.z, VNDC1.z));

            bool IsOutScreen = (LineBBoxMax.x < 0.0f || LineBBoxMax.y < 0.0f || LineBBoxMin.x > 1.0f || LineBBoxMin.y > 1.0f);
    
            if(!IsOutScreen)
            {
                //voxel z offset
                float LineDistRadio = max(dot(normalize(V0.Pos - HairBBoxMin.xyz), normalize(HairBBoxSize.xyz)), dot(normalize(V1.Pos - HairBBoxMin.xyz), normalize(HairBBoxSize.xyz)));
                uint VoxelZOffset = saturate(LineDistRadio) * (VOXEL_SLICE_NUM - 1);

                float2 StartPixelCoord = VNDC0.xy * ScreenSize;
                float2 EndPixelCoord = VNDC1.xy * ScreenSize;

                bool steep = abs(EndPixelCoord.y - StartPixelCoord.y) > abs(EndPixelCoord.x - StartPixelCoord.x);

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
                                AddToVisibilityBuffer(LineIdx0, group_idx, w_x, w_y, LineBBoxMax.z, VoxelZOffset, curr_select_tile_id0);
                            }
                        }

                        w_x = w_x + 1;
                        if(w_x > -1 && w_y < ScreenSize.y)
                        {
                            bright = frac(intersect_y);
                            if(bright > 0.0f)
                            {
                                AddToVisibilityBuffer(LineIdx0, group_idx, w_x, w_y, LineBBoxMax.z, VoxelZOffset, curr_select_tile_id1);
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
                                AddToVisibilityBuffer(LineIdx0, group_idx, w_x, w_y, LineBBoxMax.z, VoxelZOffset, curr_select_tile_id0);
                            }
                        }

                        w_y = w_y + 1;
                        if(w_x > -1 && w_y < ScreenSize.y)
                        {
                            bright = frac(intersect_y);
                            if(bright > 0.0f)
                            {
                                AddToVisibilityBuffer(LineIdx0, group_idx, w_x, w_y, LineBBoxMax.z, VoxelZOffset, curr_select_tile_id1);
                            }
                        }

                        intersect_y += gradient;
                    }
                }
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();
    if(group_idx < 2)
    {
        OutLineVisibilityBuffer[group_id.x * 2 + group_idx] = VisibilityBits[group_idx];
    }
}
