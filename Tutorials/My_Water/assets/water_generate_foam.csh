//#include "structures.fxh"

Texture2D g_DisplaccementTex;
SamplerState g_DisplaccementTex_sampler;

RWTexture2D<float> FoamTexture;

struct WaterFFTH0Uniform
{
	float4 N_L_Amplitude_Intensity; //pack
};

cbuffer Constants
{
    WaterFFTH0Uniform g_Constants;
};

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 64
#endif

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID,
          uint3 DTid : SV_DispatchThreadID)
{
	float OneTexelSize = 1.0f / THREAD_GROUP_SIZE;

	uint2 ImageIndexInt = DTid.xy;

	float3 left = g_DisplaccementTex.SampleLevel(g_DisplaccementTex_sampler, ImageIndexInt * OneTexelSize + float2(-OneTexelSize, 0.0f), 0).xyz;
	float3 right = g_DisplaccementTex.SampleLevel(g_DisplaccementTex_sampler, ImageIndexInt * OneTexelSize + float2(OneTexelSize, 0.0f), 0).xyz;
	float3 up = g_DisplaccementTex.SampleLevel(g_DisplaccementTex_sampler, ImageIndexInt * OneTexelSize + float2(0.0f, -OneTexelSize), 0).xyz;
	float3 bottom = g_DisplaccementTex.SampleLevel(g_DisplaccementTex_sampler, ImageIndexInt * OneTexelSize + float2(0.0f, OneTexelSize), 0).xyz;

	float scale = 2.0f;
	float2 Dx = (right.xy - left.xy) * scale;
	float2 Dy = (up.xy - bottom.xy) * scale;
	float J = (1.0f + Dx.x) * (1.0f + Dy.y) - Dx.y * Dy.x;
    

	FoamTexture[ImageIndexInt] = J;	
}
