
cbuffer HairConstData
{
    float4x4 ViewProj;
    float4x4 InvViewProj;
    float4 HairBBoxMin;
    float4 HairBBoxToCamMinMaxDist;
	float2 ScreenSize;
    float2 DownSampleDepthSize;
    float4 CameraForward;
    float4 CameraWPos;
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

uint PackR11G11B10F(float3 rgb)
{
    uint r = (f32tof16(rgb.r) << 7u) & 4192256u; //0000 0000 0011 1111 1111 1000 0000 0000
    uint g = (f32tof16(rgb.g) >> 4u) & 2047u;    //0000 0000 0000 0000 0000 0111 1111 1111
    uint b = (f32tof16(rgb.b) >> 5u) << 22u;
    return r | g | b;
}

float3 UnpackR11G11B10F(uint rgb)
{
    float r = f16tof32((rgb >> 7u) & 32752u); //0111 1111 1111 0000
    float g = f16tof32((rgb << 4u) & 32752u); //0111 1111 1111 0000    
    float b = f16tof32((rgb >> 22u) << 5u);
	return float3(r, g, b);
}