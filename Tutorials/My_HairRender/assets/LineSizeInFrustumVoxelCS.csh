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

RWStructuredBuffer<uint> OutLineAccumulateBuffer;

void AddAccumulateBuffer(float px, float py, float max_z, float voxel_z_offset)
{
    int DownSampleX = int(ceil(px / 16.0f));
    int DownSampleY = int(ceil(py / 16.0f));
    float OcclusionDepth = DownSampleDepthMap.Load(int2(DownSampleX, DownSampleY)).x;
    if(OcclusionDepth > max_z)
    {
        uint SrcVal;
        uint AccumuBufIdx = (DownSampleY * DownSampleDepthSize.x + DownSampleX) * VOXEL_SLICE_NUM + voxel_z_offset;
        InterlockedAdd(OutLineAccumulateBuffer[AccumuBufIdx], 1, SrcVal);
    }
}

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID, uint group_idx : SV_GroupIndex)
{
    uint LineIdx0 = id.x;
    uint VertexIdx0 = IdxData[LineIdx0];

    HairVertexData V0 = VerticesDatas[VertexIdx0];
    if(isnan(V0.Pos.x))
    {
        return;
    }

    HairVertexData V1 = VerticesDatas[VertexIdx0 + 1];

    float4 VNDC0 = float4(V0.Pos, 1.0f) * ViewProj;
    VNDC0.xy /= VNDC0.w;
    VNDC0.xy = VNDC0.xy * 0.5f + float2(0.5f);
    VNDC0.xy = saturate(VNDC0.xy);
    float4 VNDC1 = float4(V1.Pos, 1.0f) * ViewProj;
    VNDC1.xy /= VNDC1.w;
    VNDC1.xy = VNDC1.xy * 0.5f + float2(0.5f);
    VNDC1.xy = saturate(VNDC1.xy);
    if((abs(VNDC1.x - VNDC0.x) < 0.001f) && (abs(VNDC1.y - VNDC0.y) < 0.001f)) //in same pos
    {
        return;
    }

    float3 LineBBoxMin = float3(min(VNDC0.x, VNDC1.x), min(VNDC0.y, VNDC1.y), min(VNDC0.z, VNDC1.z));
    float3 LineBBoxMax = float3(max(VNDC0.x, VNDC1.x), max(VNDC0.y, VNDC1.y), max(VNDC0.z, VNDC1.z));

    //voxel z offset
    float LineDistRadio = max(dot((V0.Pos - HairBBoxMin.xyz), HairBBoxSize.xyz), dot((V1.Pos - HairBBoxMin.xyz)));
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
    if(dx > 0.0f)
    {
        gradient = dy / dx;
    }

    int s_x = StartPixelCoord.x;
    int e_x = EndPixelCoord.x;
    float intersect_y = StartPixelCoord.y;

    if(steep)
    {
        for(int i = s_x; i <= e_x; ++i)
        {
            int w_x = int(intersect_y);
            int w_y = i;
            float bright = 1.0f - frac(intersect_y);

            if(bright > 0.0f)
            {
                AddAccumulateBuffer(w_x, w_y, LineBBoxMax.z, VoxelZOffset);
            }
            //OutputTexture[int2(w_x, w_y)] = float4(bright, bright, bright, 1.0f);

            w_x = w_x + 1;
            bright = frac(intersect_y);
            //OutputTexture[int2(w_x, w_y)] = float4(bright, bright, bright, 1.0f);
            if(bright > 0.0f)
            {
                AddAccumulateBuffer(w_x, w_y, LineBBoxMax.z, VoxelZOffset);
            }

            intersect_y += gradient;
        }
    }
    else
    {
        for(int i = s_x; i <= e_x; ++i)
        {
            int w_x = i;
            int w_y = int(intersect_y);
            float bright = 1.0f - frac(intersect_y);
            if(bright > 0.0f)
            {
                AddAccumulateBuffer(w_x, w_y, LineBBoxMax.z, VoxelZOffset);
            }
            //OutputTexture[int2(w_x, w_y)] = float4(bright, bright, bright, 1.0f);

            w_y = w_y + 1;
            bright = frac(intersect_y);
            if(bright > 0.0f)
            {
                AddAccumulateBuffer(w_x, w_y, LineBBoxMax.z, VoxelZOffset);
            }
            //OutputTexture[int2(w_x, w_y)] = float4(bright, bright, bright, 1.0f);

            intersect_y += gradient;
        }
    }
}
