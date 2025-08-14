
cbuffer HairConstData
{
    float4x4 ViewProj;
    float4x4 InvViewProj;
    float4 HairBBoxMin;
    float4 HairBBoxSize;
	float2 ScreenSize;
    float2 DownSampleDepthSize;
};
