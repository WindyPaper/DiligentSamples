//Draw soft line from work queue.

#include "CommonCS.csh"

#define VOXEL_SLICE_NUM 24
#define OIT_LAYER_COUNT 8

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
groupshared uint64_t GroupMLABLayer[2048]; //16bit depth + 16bit opacity + 32bit(r11g11b10float)

uint GLPackHalf2x16(float2 value)
{
    uint2 Packed = f32tof16(value);
    return Packed.x | (Packed.y << 16);
}
float2 GLUnpackHalf2x16(uint value)
{
    return f16tof32(uint2(value & 0xffff, value >> 16));
}

bool AddAccumulateBuffer(int px, int py, float pixel_d)
{
    int tile_x = px % 16;
    int tile_y = py % 16; 
    float OcclusionDepth = GroupDepthCache[tile_y * 16 + tile_x];

    bool success = false;
    if(OcclusionDepth > (pixel_d))
    {
        success = true;
    }

    return success;
}

void WriteLine(int px, int py, float bright)
{
    bright = saturate(bright);
    OutHairRenderTex[uint2(px, py)] = float4(bright, bright, bright, 1.0f);
}

bool IsDrawLineValidPixel(int w_x, int w_y, float curr_depth)
{
    bool valid_depth = curr_depth < 1.0f && curr_depth > 0.0f;
    float occlusion_depth = GroupDepthCache[w_y * 16 + w_x];
    bool valid_area = w_x >= 0 && w_x < 16 && w_y >= 0 && w_y < 16;
    bool is_depth_pass = curr_depth < occlusion_depth;

    return valid_depth && valid_area && is_depth_pass;
}

float4 GetMLABFragmentColor(uint64_t v)
{
    uint pack_rgb = uint(v)
    float2 pr = GLUnpackHalf2x16((pack_rgb << 4u) & 32752u);
    float r = pr.x;
    float2 pg = GLUnpackHalf2x16((pack_rgb >> 7u) & 32752u);
    float g = pg.x;
    float2 pb = GLUnpackHalf2x16((pack_rgb >> 22u) << 5u);
    float b = pb.x;
    uint pack_depth_opacify = uint(v >> 32u);
    float2 p_da = GLUnpackHalf2x16(pack_depth_opacify & 65535u);
    float alpha = p_da.x;

    return float4(r, g, b, a);
}

uint64_t SetupMLABFragment(float4 rgba, float depth)
{
    uint color_int = ((GLPackHalf2x16(float2(rgba.r, 0.0)) << 7u) & 4192256u) | (((GLPackHalf2x16(float2(rgba.g, 0.0)) >> 4u) & 2047u)) | (((GLPackHalf2x16(float2(rgba.b, 0.0)) >> 5u) << 22u)))
    uint64_t depth_opacity_int = ((uint64_t(GLPackHalf2x16(float2(depth * 65535.0, 0.0))) << 16u) | (uint64_t(GLPackHalf2x16(float2(1.0 - rgba.a, 0.0))))) << 32u;

    return depth_opacity_int | color_int;
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
            //empty
        }
        else
        {
            float2 StartPixelCoord = VNDC0.xy * ScreenSize;
            float2 EndPixelCoord = VNDC1.xy * ScreenSize;

            float2 StartPixelCoordInTile = floor(StartPixelCoord) - TilePixelPos;
            float2 EndPixelCoordInTile = ceil(EndPixelCoord) - TilePixelPos;

            float MinXInTile = min(StartPixelCoordInTile.x, EndPixelCoordInTile.x);
            float MinYinTile = min(StartPixelCoordInTile.y, EndPixelCoordInTile.y);
            float MaxXInTile = max(StartPixelCoordInTile.x, EndPixelCoordInTile.x);
            float MaxYinTile = max(StartPixelCoordInTile.y, EndPixelCoordInTile.y);

            bool IsPixelInTile = (MaxXInTile > -1.0f) && (MaxYinTile > -1.0f) && (MinXInTile < 16.0f) && (MinYinTile < 16.0f);
   
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
                float dx = EndPixelCoordInTile.x - StartPixelCoordInTile.x;
                float dy = EndPixelCoordInTile.y - StartPixelCoordInTile.y;

                float gradient = 1.0f;
                if(dx != 0.0f)                
                {
                    gradient = dy / dx;
                }

                //int s_x = StartPixelCoord.x;
                //int e_x = EndPixelCoord.x;
                //float intersect_y = StartPixelCoord.y;

                float lerp_factor = 1.0f / (EndPixelCoordInTile.x - StartPixelCoordInTile.x);
                float lerp_value = 0.0f;

                int s_x_in_tile = StartPixelCoordInTile.x;
                int e_x_in_tile = EndPixelCoordInTile.x;
                float intersect_y = StartPixelCoordInTile.y;

                if(s_x_in_tile < 0)
                {
                    for(int i = s_x_in_tile; i < 0; ++i)
                    {
                        lerp_value += lerp_factor;
                        intersect_y += gradient;
                    }

                    s_x_in_tile = 0;
                }
                e_x_in_tile = min(16, e_x_in_tile);


                if(steep)
                {
                    for(int i = s_x_in_tile; i < e_x_in_tile; ++i)
                    {
                        int w_x = int(intersect_y);
                        int w_y = i;                        
                        float curr_depth = lerp(StartPixelZ, EndPixelZ, lerp_value);
                        bool is_valid_pixel = IsDrawLineValidPixel(w_x, w_y, curr_depth);
                        if(is_valid_pixel)
                        {
                            float bright = 1.0f - frac(intersect_y);
                            // if(bright > 0.0f)
                            {                            
                                WriteLine(w_x + TilePixelPos.x, w_y + TilePixelPos.y, bright);
                            }
                        }                        

                        w_x = w_x + 1;
                        is_valid_pixel = IsDrawLineValidPixel(w_x, w_y, curr_depth);
                        if(is_valid_pixel)
                        {
                            float bright = frac(intersect_y);                        
                            // if(bright > 0.0f)
                            {                                
                                WriteLine(w_x + TilePixelPos.x, w_y + TilePixelPos.y, bright);
                            }
                        }                        

                        intersect_y += gradient;
                        lerp_value += lerp_factor;
                    }
                }
                else
                {
                    for(int i = s_x_in_tile; i < e_x_in_tile; ++i)
                    {
                        int w_x = i;
                        int w_y = int(intersect_y);
                        float curr_depth = lerp(StartPixelZ, EndPixelZ, lerp_value);
                        bool is_valid_pixel = IsDrawLineValidPixel(w_x, w_y, curr_depth);
                        if(is_valid_pixel)
                        {
                            float bright = 1.0f - frac(intersect_y);
                            // if(bright > 0.0f)
                            {
                                WriteLine(w_x + TilePixelPos.x, w_y + TilePixelPos.y, bright);
                            }
                        }                        

                        w_y = w_y + 1;
                        is_valid_pixel = IsDrawLineValidPixel(w_x, w_y, curr_depth);
                        if(is_valid_pixel)
                        {
                            float bright = frac(intersect_y);
                            // if(bright > 0.0f)
                            {
                                WriteLine(w_x + TilePixelPos.x, w_y + TilePixelPos.y, bright);
                            }
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

    //init MLAB Layer
    uint64_t pack_init_mlab_data = uint64_t(GLPackHalf2x16(float2(1.0f, 1.0f)));
    uint64_t init_mlab_data = pack_init_mlab_data << 32u;
    for(int i = 0; i < OIT_LAYER_COUNT; ++i)
    {
        uint init_layer_idx = (tile_x + tile_y * 16) * OIT_LAYER_COUNT + i;
        GroupMLABLayer[init_layer_idx] = init_mlab_data;
    }

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
