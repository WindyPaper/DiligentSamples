
#include "CommonCS.csh"

// Texture2D InputSrcDepthMap;
// SamplerState InputSrcDepthMap_sampler;

RWTexture2D<float4> OutputTexture;

// void swap(inout float x, inout float y)
// {
//     float t = x;
//     x = y;
//     y = t;
// }

[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID, uint group_idx : SV_GroupIndex)
{
    float2 v0 = float2(873.709656, 282.685852);
    float2 v1 = float2(840.406616, 96.2875061);

    bool steep = abs(v1.y - v0.y) > abs(v1.x - v0.x);

    if(steep)
    {
        swap(v0.x, v0.y);
        swap(v1.x, v1.y);        
    }
    if(v0.x > v1.x)
    {
        swap(v0.x, v1.x);
        swap(v0.y, v1.y);
    }

    //compute the slope
    float dx = v1.x - v0.x;
    float dy = v1.y - v0.y;

    float gradient = 1.0f;
    if(dx > 0.0f)
    {
        gradient = dy / dx;
    }

    int s_x = v0.x;
    int e_x = v1.x;
    float intersect_y = v0.y;

    if(steep)
    {
        for(int i = s_x; i <= e_x; ++i)
        {
            int w_x = int(intersect_y);
            int w_y = i;
            float bright = 1.0f - frac(intersect_y);
            OutputTexture[int2(w_x, w_y)] = float4(bright, bright, bright, 1.0f);

            w_x = w_x + 1;
            bright = frac(intersect_y);
            OutputTexture[int2(w_x, w_y)] = float4(bright, bright, bright, 1.0f);

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
            OutputTexture[int2(w_x, w_y)] = float4(bright, bright, bright, 1.0f);

            w_y = w_y + 1;
            bright = frac(intersect_y);
            OutputTexture[int2(w_x, w_y)] = float4(bright, bright, bright, 1.0f);

            intersect_y += gradient;
        }
    }    
}
