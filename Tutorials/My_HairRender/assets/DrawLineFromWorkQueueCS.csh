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
RWTexture2D<uint4> OutDebugLayerTex;

//RWStructuredBuffer<uint> OutLineAccumulateBuffer;

StructuredBuffer<uint> WorkQueueBuffer;
ByteAddressBuffer LineSizeBuffer;
ByteAddressBuffer LineOffsetBuffer;
StructuredBuffer<uint> RenderQueueBuffer;
StructuredBuffer<uint> HairVertexShadeData;

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

uint GetDefaultInitDepthAndAlphaPackVal()
{
    return GLPackHalf2x16(float2(1.0f, 65504.0f));
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

// float4 GetMLABFragmentColorAndDepthInt(uint64_t v, out uint depth_uint)
// {
//     uint pack_rgb = uint(v);
//     float2 pr = GLUnpackHalf2x16((pack_rgb << 4u) & 32752u);
//     float r = pr.x;
//     float2 pg = GLUnpackHalf2x16((pack_rgb >> 7u) & 32752u);
//     float g = pg.x;
//     float2 pb = GLUnpackHalf2x16((pack_rgb >> 22u) << 5u);
//     float b = pb.x;
//     uint pack_depth_opacify = uint(v >> 32u);
//     float2 p_da = GLUnpackHalf2x16(pack_depth_opacify & 65535u);
//     float alpha = p_da.x;

//     depth_uint = pack_depth_opacify >> 16u;

//     return float4(r, g, b, alpha);
// }

//
//	float3 <-> u32 as f11.f11.f10
//
uint PackR11G11B10F(float3 rgb)
{
    uint r = (f32tof16(rgb.r) << 7u) & 4192256u;
    uint g = (f32tof16(rgb.g) >> 4u) & 2047u;
    uint b = (f32tof16(rgb.b) >> 5u) << 22u;
    return r | g | b;
}

float3 UnpackR11G11B10F(uint rgb)
{
    float r = f16tof32((rgb << 4u) & 32752u);
    float g = f16tof32((rgb >> 7u) & 32752u);
    float b = f16tof32((rgb >> 22u) << 5u);
	return float3(r, g, b);
}

float4 GetMLABFragmentColor(uint64_t v)
{
    uint pack_rgb = uint(v);
    float3 rgb_color = UnpackR11G11B10F(pack_rgb);
    uint pack_depth_opacify = uint(v >> 32u);
    float alpha = f16tof32(pack_depth_opacify & 65535u);

    return float4(rgb_color, alpha);
}

float2 GetMLABFragmentDepthAndAlpha(uint64_t v)
{
    return GLUnpackHalf2x16(uint(v >> 32u) & 65535u);
}

uint64_t SetupMLABFragment(float3 rgb, float alpha, float depth)
{
    uint color_int = PackR11G11B10F(rgb);//((GLPackHalf2x16(float2(rgb.r, 0.0)) << 7u) & 4192256u) | (((GLPackHalf2x16(float2(rgb.g, 0.0)) >> 4u) & 2047u)) | (((GLPackHalf2x16(float2(rgb.b, 0.0)) >> 5u) << 22u));
    uint64_t depth_opacity_int = (uint64_t(f32tof16(depth * 65535.0) << 16u) | uint64_t(f32tof16(alpha))) << 32u;

    return depth_opacity_int | color_int;
}

uint64_t SetupMLABFragmentFromDepthInt(float3 rgb, float alpha, uint depth_uint)
{
    uint color_int = PackR11G11B10F(rgb);//((GLPackHalf2x16(float2(rgb.r, 0.0)) << 7u) & 4192256u) | (((GLPackHalf2x16(float2(rgb.g, 0.0)) >> 4u) & 2047u)) | (((GLPackHalf2x16(float2(rgb.b, 0.0)) >> 5u) << 22u));
    uint64_t depth_opacity_int = ((uint64_t(depth_uint) << 16u) | uint64_t(f32tof16(alpha))) << 32u;

    return depth_opacity_int | color_int;
}

void AddToMLABLayer(uint local_x, uint local_y, float3 rgb, float alpha, float depth)
{
    uint init_depth_alpha = GetDefaultInitDepthAndAlphaPackVal();
    //uint curr_depth_alpha = GLPackHalf2x16(float2(alpha, depth));
    uint64_t curr_fragment_pack = SetupMLABFragment(rgb * alpha, 1.0f - alpha, depth);
    uint64_t blend_layer_pack_val = curr_fragment_pack;
    for(int i = 0; i < OIT_LAYER_COUNT; ++i)
    {
        uint64_t src_val;

        //top layer to low sort
        InterlockedMin(GroupMLABLayer[(local_y * 16 + local_x) * OIT_LAYER_COUNT + i], blend_layer_pack_val, src_val);

        if(src_val > blend_layer_pack_val)
        {
            blend_layer_pack_val = src_val;
        }
    }

    uint compare_layer_depth_alpha = uint(blend_layer_pack_val >> 32u);
    if(compare_layer_depth_alpha != init_depth_alpha)
    {
        //blend to last layer        
        float4 blend_color = GetMLABFragmentColor(blend_layer_pack_val);

        for(int i = 0; i < 128;)
        {
            uint64_t last_layer_pack = GroupMLABLayer[(local_y * 16 + local_x) * OIT_LAYER_COUNT + OIT_LAYER_COUNT - 1];            
            float4 last_layer_color = GetMLABFragmentColor(last_layer_pack);
            uint last_layer_depth_uint = uint(last_layer_pack >> 48u);

            float3 merge_color = last_layer_color.rgb + blend_color.rgb * last_layer_color.a;
            float merge_color_alpha = last_layer_color.a * blend_color.a;

            uint64_t merge_color_pack = SetupMLABFragmentFromDepthInt(merge_color, merge_color_alpha, last_layer_depth_uint);

            uint64_t src_layer_pack_val;
            InterlockedCompareExchange(GroupMLABLayer[(local_y * 16 + local_x) * OIT_LAYER_COUNT + OIT_LAYER_COUNT - 1], 
                last_layer_pack,
                merge_color_pack,
                src_layer_pack_val
            );

            if(src_layer_pack_val == last_layer_pack)
            {
                //successful, break
                i += 255;
            }

            ++i;
        }

    }
}

void DrawSoftLine(HairVertexData V0, HairVertexData V1, float3 HairColor0, float3 HairColor1, uint2 TilePixelPos, uint TileId)
{
    if(!isnan(V0.Pos.x))
    {
        float4 VNDC0 = mul(float4(V0.Pos, 1.0f), ViewProj);
        VNDC0.xyz /= VNDC0.w;
        VNDC0.xy = VNDC0.xy * 0.5f + float2(0.5f, 0.5f);
        //VNDC0.xy = saturate(VNDC0.xy);
        float4 VNDC1 = mul(float4(V1.Pos, 1.0f), ViewProj);
        VNDC1.xyz /= VNDC1.w;
        VNDC1.xy = VNDC1.xy * 0.5f + float2(0.5f, 0.5f);
        //VNDC1.xy = saturate(VNDC1.xy);
        if((abs(VNDC1.x - VNDC0.x) < 0.001f) && (abs(VNDC1.y - VNDC0.y) < 0.001f)) //in same pos
        {
            //empty
        }
        else
        {
            float2 StartPixelCoord = (VNDC0.xy * ScreenSize);
            float2 EndPixelCoord = (VNDC1.xy * ScreenSize);

            float2 StartPixelCoordInTile = (StartPixelCoord) - TilePixelPos;
            float2 EndPixelCoordInTile = (EndPixelCoord) - TilePixelPos;

            float MinXInTile = min(StartPixelCoordInTile.x, EndPixelCoordInTile.x);
            float MinYinTile = min(StartPixelCoordInTile.y, EndPixelCoordInTile.y);
            float MaxXInTile = max(StartPixelCoordInTile.x, EndPixelCoordInTile.x);
            float MaxYinTile = max(StartPixelCoordInTile.y, EndPixelCoordInTile.y);

            bool IsPixelInTile = (MaxXInTile > -1.0f) && (MaxYinTile > -1.0f) && (MinXInTile < 16.0f) && (MinYinTile < 16.0f);
   
            if(IsPixelInTile)
            {
                float StartPixelZ = VNDC0.z;
                float EndPixelZ = VNDC1.z;

                bool steep = abs(EndPixelCoordInTile.y - StartPixelCoordInTile.y) > abs(EndPixelCoordInTile.x - StartPixelCoordInTile.x);

                if(steep)
                {
                    swap(StartPixelCoord.x, StartPixelCoord.y);
                    swap(EndPixelCoord.x, EndPixelCoord.y);

                    swap(StartPixelCoordInTile.x, StartPixelCoordInTile.y);
                    swap(EndPixelCoordInTile.x, EndPixelCoordInTile.y);

                }
                if(StartPixelCoordInTile.x > EndPixelCoordInTile.x)
                {
                    swap(StartPixelCoord.x, EndPixelCoord.x);
                    swap(StartPixelCoord.y, EndPixelCoord.y);

                    swap(StartPixelCoordInTile.x, EndPixelCoordInTile.x);
                    swap(StartPixelCoordInTile.y, EndPixelCoordInTile.y);

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

                int s_x = StartPixelCoord.x;
                int e_x = EndPixelCoord.x;
                float intersect_y_not_in_tile = StartPixelCoord.y;
                float test_accu_gradient = 0.0f;

                float lerp_factor = 1.0f / (EndPixelCoordInTile.x - StartPixelCoordInTile.x);
                float lerp_value = 0.0f;

                float round_start_tile_x = round(StartPixelCoordInTile.x);
                float round_end_tile_x = round(EndPixelCoordInTile.x);

                int s_x_in_tile = round_start_tile_x;//(StartPixelCoordInTile.x);
                int e_x_in_tile = round_end_tile_x;//(EndPixelCoordInTile.x);
                float intersect_y = (StartPixelCoordInTile.y);// +  gradient * (round_start_tile_x - StartPixelCoordInTile.x);
                
                // uint loop_num = 0;
                while(s_x_in_tile < 0 && intersect_y < 0.0f)
                {
                    lerp_value += lerp_factor;
                    intersect_y += gradient;
                    
                    ++s_x_in_tile;
                    // ++loop_num;
                }
                e_x_in_tile = min(16, e_x_in_tile);

                //float3 test_white_color = float3(1.0f, 1.0f, 1.0f);

                if(steep)
                {
                    for(int i = s_x_in_tile; i < e_x_in_tile; ++i)
                    {
                        int w_x = floor(intersect_y);
                        int w_y = i;                        
                        float curr_depth = lerp(StartPixelZ, EndPixelZ, lerp_value);
                        bool is_valid_pixel = IsDrawLineValidPixel(w_x, w_y, curr_depth);
                        if(is_valid_pixel)
                        {
                            float bright = 1.0f - frac(intersect_y);
                            // if(bright > 0.0f)
                            // {                                                            
                            //     OutHairRenderTex[uint2(w_x + TilePixelPos.x, w_y + TilePixelPos.y)] = float4(test_white_color * bright, 1.0f);
                            // }
                            float3 hair_lerp_color = lerp(HairColor0, HairColor1, lerp_value);
                            AddToMLABLayer(w_x, w_y, hair_lerp_color, bright, curr_depth);
                        }                        

                        w_x = w_x + 1;
                        is_valid_pixel = IsDrawLineValidPixel(w_x, w_y, curr_depth);
                        if(is_valid_pixel)
                        {
                            float bright = frac(intersect_y);                        
                            // if(bright > 0.0f)
                            // {
                            //     OutHairRenderTex[uint2(w_x + TilePixelPos.x, w_y + TilePixelPos.y)] = float4(test_white_color * bright, 1.0f);
                            // }
                            float3 hair_lerp_color = lerp(HairColor0, HairColor1, lerp_value);
                            AddToMLABLayer(w_x, w_y, hair_lerp_color, bright, curr_depth);
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
                        int w_y = floor(intersect_y);
                        float curr_depth = lerp(StartPixelZ, EndPixelZ, lerp_value);
                        bool is_valid_pixel = IsDrawLineValidPixel(w_x, w_y, curr_depth);
                        if(is_valid_pixel)
                        {
                            float bright = 1.0f - frac(intersect_y);
                            // if(bright > 0.0f)
                            // {
                            //     OutHairRenderTex[uint2(w_x + TilePixelPos.x, w_y + TilePixelPos.y)] = float4(test_white_color * bright, 1.0f);
                            // }
                            float3 hair_lerp_color = lerp(HairColor0, HairColor1, lerp_value);
                            AddToMLABLayer(w_x, w_y, hair_lerp_color, bright, curr_depth);
                        }                        

                        w_y = w_y + 1;
                        is_valid_pixel = IsDrawLineValidPixel(w_x, w_y, curr_depth);
                        if(is_valid_pixel)
                        {
                            float bright = frac(intersect_y);
                            // if(bright > 0.0f)
                            // {
                            //     OutHairRenderTex[uint2(w_x + TilePixelPos.x, w_y + TilePixelPos.y)] = float4(test_white_color * bright, 1.0f);
                            // }
                            float3 hair_lerp_color = lerp(HairColor0, HairColor1, lerp_value);
                            AddToMLABLayer(w_x, w_y, hair_lerp_color, bright, curr_depth);
                        }                        

                        intersect_y += gradient;
                        lerp_value += lerp_factor;
                    }
                }

                //test
                // if(steep)
                // {                    
                //     for(int i = s_x; i < e_x; ++i)
                //     {
                //         int w_x = int(intersect_y_not_in_tile);
                //         int w_y = i;                                                                        
                //         float bright = 1.0f - frac(intersect_y_not_in_tile);
                //         if(bright > 0.0f)
                //         {                            
                //             OutDebugLayerTex[uint2(w_x, w_y)] = float4(bright, i - s_x, test_accu_gradient, TileId);
                //         }                                                                      

                //         w_x = w_x + 1;                        
                //         bright = frac(intersect_y_not_in_tile);                        
                //         if(bright > 0.0f)
                //         {                                
                //             OutDebugLayerTex[uint2(w_x, w_y)] = float4(bright, i - s_x, test_accu_gradient, TileId);
                //         }                                                                                            

                //         intersect_y_not_in_tile += gradient;   
                //         test_accu_gradient += gradient;                     
                //     }
                // }
                // else
                // {
                //     for(int i = s_x_in_tile; i < e_x_in_tile; ++i)
                //     {
                //         int w_x = i;
                //         int w_y = int(intersect_y_not_in_tile);                        
                //         float bright = 1.0f - frac(intersect_y_not_in_tile);
                //         if(bright > 0.0f)
                //         {
                //             OutDebugLayerTex[uint2(w_x, w_y)] = float4(bright, bright, test_accu_gradient, TileId);
                //         }                        

                //         w_y = w_y + 1;                        
                //         bright = frac(intersect_y_not_in_tile);
                //         if(bright > 0.0f)
                //         {
                //             OutDebugLayerTex[uint2(w_x, w_y)] = float4(bright, bright, test_accu_gradient, TileId);
                //         }

                //         intersect_y_not_in_tile += gradient;
                //         test_accu_gradient += gradient;
                //     }
                // }
            }
        }        
    }
}

void BlendMLABToOutput(uint local_x, uint local_y, uint2 screen_pixel_pos)
{
    uint64_t local_layer_pack_vals[OIT_LAYER_COUNT];

    for(int i = 0; i < OIT_LAYER_COUNT; ++i)
    {
        local_layer_pack_vals[i] = GroupMLABLayer[(local_y * 16 + local_x) * OIT_LAYER_COUNT + i];
    }

    float4 output_color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    uint init_depth_alpha = GetDefaultInitDepthAndAlphaPackVal();
    if((local_layer_pack_vals[0] >> 32u) != init_depth_alpha)
    {
        float3 accumu_color = float3(0.0f, 0.0f, 0.0f);
        float blend_alpha = 1.0f;
        for(int i = 0; i < OIT_LAYER_COUNT; ++i)
        {            
            float4 layer_color = GetMLABFragmentColor(local_layer_pack_vals[i]) * blend_alpha;            

            accumu_color += layer_color.rgb;
            blend_alpha *= layer_color.a;

            //test
            // break;

            if(i + 1 < OIT_LAYER_COUNT)
            {
                if((local_layer_pack_vals[i + 1] >> 32u) == init_depth_alpha)
                {
                    break;
                }
            }
        }

        output_color = float4(accumu_color, blend_alpha);
    }

    OutHairRenderTex[screen_pixel_pos] = output_color;
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
    //max fp16 value: 65504
    uint64_t pack_init_mlab_data = uint64_t(GetDefaultInitDepthAndAlphaPackVal());
    uint64_t init_mlab_data = pack_init_mlab_data << 32u;
    for(int i = 0; i < OIT_LAYER_COUNT; ++i)
    {
        uint init_layer_idx = (local_thread_id.x + local_thread_id.y * 16) * OIT_LAYER_COUNT + i;
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

    OutDebugLayerTex[screen_pixel_pos.xy] = uint4(0, 0, 0, 0);//(0.0f, 0.0f, 0.0f, 0.0f);
    uint test_val = (tile_y * DownSampleDepthSize.x + tile_x) * VOXEL_SLICE_NUM + valid_offset_buf_idx;    

    GroupMemoryBarrierWithGroupSync();

    OutDebugLayerTex[screen_pixel_pos.xy].z = 10086;

    // for slice iterator
    for(uint curr_slice_num = valid_offset_buf_idx; curr_slice_num < VOXEL_SLICE_NUM; ++curr_slice_num)
    {
        uint voxel_data_idx = (tile_y * DownSampleDepthSize.x + tile_x) * VOXEL_SLICE_NUM + curr_slice_num;
        uint offset_idx = LineOffsetBuffer.Load(voxel_data_idx * 4);
        uint line_size = LineSizeBuffer.Load(voxel_data_idx * 4);

        OutDebugLayerTex[screen_pixel_pos.xy].xy = uint2(voxel_data_idx, test_val); 
        
        if(line_size > 0)
        {
            // OutHairRenderTex[screen_pixel_pos.xy] = float4(0.0f, 0.0f, id.x, id.y); 
            OutDebugLayerTex[screen_pixel_pos.xy].z = voxel_data_idx;
            ++OutDebugLayerTex[screen_pixel_pos.xy].w;
            for(uint curr_line_idx = thread_group_idx; curr_line_idx < line_size; curr_line_idx += 16 * 16)
            {                
                uint line_idx = RenderQueueBuffer[offset_idx + curr_line_idx];
                uint VertexIdx0 = IdxData[line_idx] & 0x0FFFFFFF;
                uint VertexIdx1 = VertexIdx0 + 1;

                HairVertexData V0 = VerticesDatas[VertexIdx0];
                HairVertexData V1 = VerticesDatas[VertexIdx1];

                uint HairPackC0 = HairVertexShadeData[VertexIdx0];
                uint HairPackC1 = HairVertexShadeData[VertexIdx1];

                float3 HairColor0;
                HairColor0.x = GLUnpackHalf2x16((HairPackC0 << 4u) & 32752u).x;
                HairColor0.y = GLUnpackHalf2x16((HairPackC0 >> 7u) & 32752u).x;
                HairColor0.z = GLUnpackHalf2x16((HairPackC0 >> 22u) << 5u).x;

                float3 HairColor1;
                HairColor1.x = GLUnpackHalf2x16((HairPackC1 << 4u) & 32752u).x;
                HairColor1.y = GLUnpackHalf2x16((HairPackC1 >> 7u) & 32752u).x;
                HairColor1.z = GLUnpackHalf2x16((HairPackC1 >> 22u) << 5u).x;

                // HairColor0 = float3(0.0f, 0.0f, 0.0f);
                // HairColor1 = float3(1.0f, 1.0f, 1.0f);

                DrawSoftLine(V0, V1, HairColor0, HairColor1, uint2(tile_x_in_pixel, tile_y_in_pixel), tile_y * 16 + tile_x);
            }

            // //blend 
            //GroupMemoryBarrierWithGroupSync();

            // BlendMLABToOutput(local_thread_id.x, local_thread_id.y, screen_pixel_pos);
        }

        //blend 
        GroupMemoryBarrierWithGroupSync();

        BlendMLABToOutput(local_thread_id.x, local_thread_id.y, screen_pixel_pos);
    }
    


}
