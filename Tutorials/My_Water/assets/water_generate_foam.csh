//#include "structures.fxh"

Texture2D g_DisplaccementTex;
SamplerState g_DisplaccementTex_sampler;

RWTexture2D<float4> FoamTexture;

struct WaterFFTH0Uniform
{
	float4 N_L_Amplitude_Intensity; //pack
	float4 WindDir_LL_Alignment; //pack
};

cbuffer Constants
{
    WaterFFTH0Uniform g_Constants;
};

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 64
#endif

[numthreads(1, THREAD_GROUP_SIZE, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID,
          uint3 DTid : SV_DispatchThreadID)
{
	uint2 ImageIndexInt = DTid.xy;

    

	FoamTexture[ImageIndexInt] = float4(1, 1, 0, 1);	
}
