
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

float lineWu(float2 a, float2 b, float2 c)
{   
    float minX = min(a.x, b.x);
    float maxX = max(a.x, b.x);
    float minY = min(a.y, b.y);
    float maxY = max(a.y, b.y);
        
    // crop line segment
    if (c.x < floor(minX) || c.x > ceil(maxX))
        return 0.0;
    if (c.y < floor(minY) || c.y > ceil(maxY))
        return 0.0;
    
    // swap X and Y
	float2 d = float2(abs(a.x - b.x), abs(a.y - b.y));
    if (d.y > d.x) 
    {
        a = a.yx;
        b = b.yx;
        c = c.yx;
        d = d.yx;
        
        minX = minY;
        maxX = maxY;
    } 
    
    // handle end points
    float k = 1.0;
    if (c.x == floor(minX))
        k = 1.0 - frac(minX);
    if (c.x == ceil(maxX))
        k = frac(maxX);
    

    // find Y by two-point form of linear equation
    // http://en.wikipedia.org/wiki/Linear_equation#Two-point_form
    float y = (b.y - a.y) / (b.x - a.x) * (c.x - a.x) + a.y;

    // calculate result brightness
    float br = 1.0 - abs(c.y - y);
   	return max(0.0, br) * k * (length(d) / d.x);
}


[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID, uint group_idx : SV_GroupIndex)
{
    float2 v0 = float2(282.685852, 873.709656);
    float2 v1 = float2(96.2875061, 840.406616);

    float bright = lineWu(v0, v1, id.xy) * 1.5f;
    OutputTexture[int2(id.xy)] = float4(bright, bright, bright, 1.0f);
}
