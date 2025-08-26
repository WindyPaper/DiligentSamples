//Draw soft line from work queue.

#include "CommonCS.csh"

#define VOXEL_SLICE_NUM 24

struct HairVertexData
{
    float3 Pos;
    int Misc;
};
StructuredBuffer<HairVertexData> VerticesDatas;
StructuredBuffer<uint> IdxData;

Texture2D<float4> FullDepthMap;
RWTexture2D<float4> OutHairRenderTex;

//RWStructuredBuffer<uint> OutLineAccumulateBuffer;

StructuredBuffer<uint> WorkQueueBuffer;
StructuredBuffer<uint> LineSizeBuffer;
StructuredBuffer<uint> LineOffsetBuffer;
StructuredBuffer<uint> RenderQueueBuffer;

groupshared float GroupDepthCache[256];
groupshared uint GroupPixelVisibilityBit[8];


bool AddAccumulateBuffer(int px, int py, float pixel_d)
{
    int tile_x = px % 16;
    int tile_y = py % 16; 
    float OcclusionDepth = GroupDepthCache[tile_y * 16 + tile_x];

    bool success = false;
    if(OcclusionDepth > pixel_d)
    {
        success = true;
    }

    return success;
}

void WriteLine(int px, int py, float bright, float depth)
{
    bool success = AddAccumulateBuffer(px, py, depth);

    if(success)
    {
        OutHairRenderTex[uint2(px, py)] = float4(bright, bright, bright, 1.0f);
    }
}

void DrawSoftLine(HairVertexData V0, HairVertexData V1, uint2 TilePixelPos)
{
    if(!isnan(V0.Pos.x))
    {
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
            //return;
            //empty
        }
        else
        {
            // float3 LineBBoxMin = float3(min(VNDC0.x, VNDC1.x), min(VNDC0.y, VNDC1.y), min(VNDC0.z, VNDC1.z));
            // float3 LineBBoxMax = float3(max(VNDC0.x, VNDC1.x), max(VNDC0.y, VNDC1.y), max(VNDC0.z, VNDC1.z));

            //voxel z offset
            // float LineDistRadio = max(dot(normalize(V0.Pos - HairBBoxMin.xyz), normalize(HairBBoxSize.xyz)), dot(normalize(V1.Pos - HairBBoxMin.xyz), normalize(HairBBoxSize.xyz)));
            // uint VoxelZOffset = saturate(LineDistRadio) * (VOXEL_SLICE_NUM - 1);

            float2 StartPixelCoord = VNDC0.xy * ScreenSize;
            float2 EndPixelCoord = VNDC1.xy * ScreenSize;

            float2 StartPixelCoordInTile = (StartPixelCoord) - TilePixelPos;
            float2 EndPixelCoordInTile = (EndPixelCoord) - TilePixelPos;

            float MinXInTile = min(StartPixelCoordInTile.x, EndPixelCoordInTile.x);
            float MinYinTile = min(StartPixelCoordInTile.y, EndPixelCoordInTile.y);
            float MaxXInTile = max(StartPixelCoordInTile.x, EndPixelCoordInTile.x);
            float MaxYinTile = max(StartPixelCoordInTile.y, EndPixelCoordInTile.y);

            bool IsPixelInTile = (MinXInTile > -1.0f) && (MinYinTile > -1.0f) && (MaxXInTile < 16.0f) && (MaxYinTile < 16.0f);

            if(IsPixelInTile)
            {
                float StartPixelZ = VNDC0.z;
                float EndPixelZ = VNDC1.z;

                bool steep = abs(EndPixelCoord.y - StartPixelCoord.y) > abs(EndPixelCoord.x - StartPixelCoord.x);

                if(steep)
                {
                    swap(StartPixelCoord.x, StartPixelCoord.y);
                    swap(EndPixelCoord.x, EndPixelCoord.y);

                    swap_uint(StartPixelCoordInTile.x, StartPixelCoordInTile.y);
                    swap_uint(EndPixelCoordInTile.x, EndPixelCoordInTile.y);
                }
                if(StartPixelCoord.x > EndPixelCoord.x)
                {
                    swap(StartPixelCoord.x, EndPixelCoord.x);
                    swap(StartPixelCoord.y, EndPixelCoord.y);

                    swap_uint(StartPixelCoordInTile.x, EndPixelCoordInTile.x);
                    swap_uint(StartPixelCoordInTile.y, EndPixelCoordInTile.y);

                    swap(StartPixelZ, EndPixelZ);
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

                float lerp_factor = 1.0f / (EndPixelCoordInTile.x - StartPixelCoordInTile.x);
                float lerp_value = 0.0f;

                if(steep)
                {
                    for(int i = s_x; i <= e_x; ++i)
                    {
                        int w_x = int(intersect_y);
                        int w_y = i;
                        float bright = 1.0f - frac(intersect_y);

                        if(bright > 0.0f)
                        {
                            float curr_depth = lerp(StartPixelZ, EndPixelZ, lerp_value);
                            WriteLine(w_x, w_y, bright, curr_depth);
                        }                        

                        w_x = w_x + 1;
                        bright = frac(intersect_y);                        
                        if(bright > 0.0f)
                        {
                            float curr_depth = lerp(StartPixelZ, EndPixelZ, lerp_value);
                            WriteLine(w_x, w_y, bright, curr_depth);
                        }

                        intersect_y += gradient;
                        lerp_value += lerp_factor;
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
                            float curr_depth = lerp(StartPixelZ, EndPixelZ, lerp_value);
                            WriteLine(w_x, w_y, bright, curr_depth);
                        }                        

                        w_y = w_y + 1;
                        bright = frac(intersect_y);
                        if(bright > 0.0f)
                        {
                            float curr_depth = lerp(StartPixelZ, EndPixelZ, lerp_value);
                            WriteLine(w_x, w_y, bright, curr_depth);
                        }

                        intersect_y += gradient;
                        lerp_value += lerp_factor;
                    }
                }
            }
        }        
    }
}

//16X16 pixel per tile
[numthreads(16, 16, 1)]
void CSMain(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID, \
    uint thread_group_idx : SV_GroupIndex, uint3 local_thread_id : SV_GroupThreadID)
{
    uint work_queue_idx = group_id.x;

    uint work_queue_value = WorkQueueBuffer[work_queue_idx];
    uint tile_x = work_queue_value & 4095u;
    uint tile_y = (work_queue_value >> 12u) & 4095u;
    uint valid_offset_buf_idx = work_queue_value >> 24u;

    // uint voxel_data_idx = (tile_y * DownSampleDepthSize.x + tile_x) * VOXEL_SLICE_NUM + valid_offset_buf_idx;
    uint tile_x_in_pixel = tile_x * 16;
    uint tile_y_in_pixel = tile_y * 16;
    uint2 screen_pixel_pos = uint2(tile_x_in_pixel + local_thread_id.x, tile_y_in_pixel + local_thread_id.y);

    GroupDepthCache[local_thread_id.y * 16 + local_thread_id.x] = FullDepthMap.Load(int3(screen_pixel_pos, 0)).x;

    if(thread_group_idx < 8u)
    {
        GroupPixelVisibilityBit[thread_group_idx] = 0u;
    }

    GroupMemoryBarrier();

    // for slice iterator
    for(uint curr_slice_num = valid_offset_buf_idx; curr_slice_num < VOXEL_SLICE_NUM; ++ curr_slice_num)
    {
        uint voxel_data_idx = (tile_y * DownSampleDepthSize.x + tile_x) * VOXEL_SLICE_NUM + curr_slice_num;
        uint offset_idx = LineOffsetBuffer[voxel_data_idx];
        if(offset_idx > 0)
        {
            for(uint curr_line_idx = thread_group_idx; curr_line_idx < LineSizeBuffer[voxel_data_idx]; curr_line_idx += 16 * 16)
            {
                uint line_idx = RenderQueueBuffer[offset_idx + curr_line_idx];
                uint VertexIdx0 = IdxData[line_idx] & 0x0FFFFFFF;
                uint VertexIdx1 = VertexIdx0 + 1;

                HairVertexData V0 = VerticesDatas[VertexIdx0];
                HairVertexData V1 = VerticesDatas[VertexIdx1];

                DrawSoftLine(V0, V1, uint2(tile_x_in_pixel, tile_y_in_pixel));            
            }
        }
    }
}
