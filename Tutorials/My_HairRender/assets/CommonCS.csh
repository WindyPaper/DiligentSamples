
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
