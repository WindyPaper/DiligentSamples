//#include "structures.fxh"
#include "complex.fxh"

static const float M_PI = 3.1415926535897932384626433832795;
static const float g = 9.81;

RWBuffer<float4> pingpong0;

RWBuffer<float4> pingpong1;

Buffer<float4> TwiddleIndices;

//StructuredBuffer<int> bit_reversed;

struct WaterFFTButterflyUniform
{
	float4 Stage_PingPong_Direction_Padding;
};

cbuffer Constants
{
    WaterFFTHKTUniform g_Constants;
};

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 64
#endif

void horizontalButterflies(uint2 index)
{
	complex H;
	int2 x = index;
	
	if(pingpong == 0)
	{
		vec4 data = imageLoad(TwiddleIndices, int3(stage, x.x)).rgba;
		vec2 p_ = imageLoad(pingpong0, int3(data.z, x.y)).rg;
		vec2 q_ = imageLoad(pingpong0, int3(data.w, x.y)).rg;
		vec2 w_ = vec2(data.x, data.y);
		
		complex p = complex(p_.x,p_.y);
		complex q = complex(q_.x,q_.y);
		complex w = complex(w_.x,w_.y);
		
		//Butterfly operation
		H = add(p,mul(w,q));
		
		imageStore(pingpong1, x, vec4(H.real, H.im, 0, 1));
	}
	else if(pingpong == 1)
	{
		vec4 data = imageLoad(TwiddleIndices, int3(stage, x.x)).rgba;
		vec2 p_ = imageLoad(pingpong1, int3(data.z, x.y)).rg;
		vec2 q_ = imageLoad(pingpong1, int3(data.w, x.y)).rg;
		vec2 w_ = vec2(data.x, data.y);
		
		complex p = complex(p_.x,p_.y);
		complex q = complex(q_.x,q_.y);
		complex w = complex(w_.x,w_.y);
		
		//Butterfly operation
		H = add(p,mul(w,q));
		
		imageStore(pingpong0, x, vec4(H.real, H.im, 0, 1));
	}
}

void verticalButterflies(uint2 index)
{
	complex H;
	int2 x = index;
	
	if(pingpong == 0)
	{
		vec4 data = imageLoad(TwiddleIndices, int3(stage, x.y)).rgba;
		vec2 p_ = imageLoad(pingpong0, int3(x.x, data.z)).rg;
		vec2 q_ = imageLoad(pingpong0, int3(x.x, data.w)).rg;
		vec2 w_ = vec2(data.x, data.y);
		
		complex p = complex(p_.x,p_.y);
		complex q = complex(q_.x,q_.y);
		complex w = complex(w_.x,w_.y);
		
		//Butterfly operation
		H = add(p,mul(w,q));
		
		imageStore(pingpong1, x, vec4(H.real, H.im, 0, 1));
	}
	else if(pingpong == 1)
	{
		vec4 data = imageLoad(TwiddleIndices, int3(stage, x.y)).rgba;
		vec2 p_ = imageLoad(pingpong1, int3(x.x, data.z)).rg;
		vec2 q_ = imageLoad(pingpong1, int3(x.x, data.w)).rg;
		vec2 w_ = vec2(data.x, data.y);
		
		complex p = complex(p_.x,p_.y);
		complex q = complex(q_.x,q_.y);
		complex w = complex(w_.x,w_.y);
		
		//Butterfly operation
		H = add(p,mul(w,q));
		
		imageStore(pingpong0, x, vec4(H.real, H.im, 0, 1));
	}
}

[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID)
{
	int direction = int(g_Constants.Stage_PingPong_Direction_Padding.z);
	uint2 ImageIndexInt = GTid.xy;

	if(direction == 0)
		horizontalButterflies(ImageIndexInt);
	else if(direction == 1)
		verticalButterflies(ImageIndexInt);
}

// [numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
// void main(uint3 Gid  : SV_GroupID,
//           uint3 GTid : SV_GroupThreadID)
// {
// 	float N = g_Constants.N_padding.x;

// 	uint2 ImageIndexInt = GTid.xy;

//     uint uiGlobalThreadIdx = GTid.y * uint(THREAD_GROUP_SIZE) + GTid.x;
//     // if (uiGlobalThreadIdx >= g_Constants.uiNumParticles)
//     //     return;    
//     float k = fmod(ImageIndexInt.y * (float(N)/ pow(2,ImageIndexInt.x+1)), N);
// 	complex twiddle;
// 	twiddle.real = cos(2.0*M_PI*k/float(N));
// 	twiddle.im = sin(2.0*M_PI*k/float(N));
	
// 	int butterflyspan = int(pow(2, ImageIndexInt.x));
	
// 	int butterflywing;
	
// 	if (fmod(ImageIndexInt.y, pow(2, ImageIndexInt.x + 1)) < pow(2, ImageIndexInt.x))
// 		butterflywing = 1;
// 	else butterflywing = 0;

// 	// first stage, bit reversed indices
// 	if (ImageIndexInt.x == 0) {
// 		// top butterfly wing
// 		if (butterflywing == 1)
// 			TwiddleIndices[uiGlobalThreadIdx] = float4(twiddle.real, twiddle.im, bit_reversed[int(ImageIndexInt.y)], bit_reversed[int(ImageIndexInt.y + 1)]);
// 		// bot butterfly wing
// 		else	
// 			TwiddleIndices[uiGlobalThreadIdx] = float4(twiddle.real, twiddle.im, bit_reversed[int(ImageIndexInt.y - 1)], bit_reversed[int(ImageIndexInt.y)]);
// 	}
// 	// second to log2(N) stage
// 	else {
// 		// top butterfly wing
// 		if (butterflywing == 1)
// 			TwiddleIndices[uiGlobalThreadIdx] = float4(twiddle.real, twiddle.im, ImageIndexInt.y, ImageIndexInt.y + butterflyspan);
// 		// bot butterfly wing
// 		else
// 			TwiddleIndices[uiGlobalThreadIdx] = float4(twiddle.real, twiddle.im, ImageIndexInt.y - butterflyspan, ImageIndexInt.y);
// 	}
// }
