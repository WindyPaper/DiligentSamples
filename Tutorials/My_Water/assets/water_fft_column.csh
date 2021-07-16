//#include "structures.fxh"

static const float M_PI = 3.1415926535897932384626433832795;
static const float g = 9.81;

Texture2D H0;

RWTexture2D<float2> HtOutput;
RWTexture2D<float4> DtOutput;

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

[numthreads(THREAD_GROUP_SIZE/2, 1, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID,
          uint3 DTid : SV_DispatchThreadID)
{
	uint2 ImageIndexInt = DTid.xy;

    //uint uiGlobalThreadIdx = GTid.y * uint(THREAD_GROUP_SIZE) + GTid.x;
    // if (uiGlobalThreadIdx >= g_Constants.uiNumParticles)
    //     return;

    
}
