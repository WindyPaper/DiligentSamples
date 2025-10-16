
cbuffer HairConstData
{
    float4x4 ViewProj;
    float4x4 InvViewProj;
    float4 HairBBoxMin;
    float4 HairBBoxSize;
	float2 ScreenSize;
    float2 DownSampleDepthSize;
};

void swap(inout float x, inout float y)
{
    float t = x;
    x = y;
    y = t;
}

void swap_uint(inout uint x, inout uint y)
{
    uint t = x;
    x = y;
    y = t;
}

void swap_f3(inout float3 x, inout float3 y)
{
    float3 t = x;
    x = y;
    y = t;
}

bool IsValidPixel(int w_x, int w_y)
{
    bool ret = false;
    if(w_x > -1 && w_x < ScreenSize.x && w_y > -1 && w_y < ScreenSize.y)
    {
        ret = true;
    }

    return ret;
}