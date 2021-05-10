#include "complex.fxh"

Texture2D pingpong0;

Texture2D pingpong1;

RWTexture2D<float4> displacement;

//StructuredBuffer<int> bit_reversed;

struct WaterFFTInvsersionUniform
{
	float4 PingPong_N_Padding;
};

cbuffer Constants
{
    WaterFFTInvsersionUniform g_Constants;
};

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 64
#endif

[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID)
{
	int pingpong = int(g_Constants.x);
	int N = int(g_Constants.y);

	uint2 ImageIndexInt = GTid.xy;

	float perms[] = {1.0,-1.0};
	int index = int(mod((int(ImageIndexInt.x + ImageIndexInt.y)),2));
	float perm = perms[index];
	
	if(pingpong == 0)
	{
		float h = pingpong0.Load(int3(x, 0.0)).r;
		displacement[x] = float4(perm*(h/float(N*N)), perm*(h/float(N*N)), perm*(h/float(N*N)), 1);
	}
	else if(pingpong == 1)
	{
		float h = pingpong1.Load(int3(x, 0.0)).r;
		displacement[x] = float4(perm*(h/float(N*N)), perm*(h/float(N*N)), perm*(h/float(N*N)), 1);
	}
}