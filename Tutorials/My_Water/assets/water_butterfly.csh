//#include "structures.fxh"
#include "complex.fxh"

static const float M_PI = 3.1415926535897932384626433832795;
static const float g = 9.81;

RWTexture2D<float4> pingpong0;

RWTexture2D<float4> pingpong1;

Texture2D TwiddleIndices;

//StructuredBuffer<int> bit_reversed;

struct WaterFFTButterflyUniform
{
	float4 Stage_PingPong_Direction_Padding;
};

cbuffer Constants
{
    WaterFFTButterflyUniform g_Constants;
};

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 64
#endif

groupshared float2 HData[256];

void horizontalButterflies(uint2 index)
{
	complex H;
	uint2 x = index;

	//int stage = int(g_Constants.Stage_PingPong_Direction_Padding.x);
	int pingpong = int(g_Constants.Stage_PingPong_Direction_Padding.y);	
	
	if(pingpong == 0)
	{
		for(int stage = 0; stage < 8; ++stage)
		{
			float4 data = TwiddleIndices.Load(int3(stage, x.x, 0)).rgba;

			float2 p_, q_;

			if(stage == 0)
			{
				p_ = pingpong0.Load(int3(data.z, x.y, 0)).rg;
				q_ = pingpong0.Load(int3(data.w, x.y, 0)).rg;
			}
			else
			{
				p_ = HData[data.z];
				q_ = HData[data.w];

				GroupMemoryBarrierWithGroupSync();
			}			
			float2 w_ = float2(data.x, data.y);
			
			complex p = {p_.x,p_.y};
			complex q = {q_.x,q_.y};
			complex w = {w_.x,w_.y};
			
			//Butterfly operation
			complex r = add(p,mul(w,q));
			HData[x.x] = float2(r.real, r.im);

			GroupMemoryBarrierWithGroupSync();
		}
		// float4 data = TwiddleIndices.Load(int3(stage, x.x, 0)).rgba;
		// float2 p_ = pingpong0.Load(int3(data.z, x.y, 0)).rg;
		// float2 q_ = pingpong0.Load(int3(data.w, x.y, 0)).rg;
		// float2 w_ = float2(data.x, data.y);
		
		// complex p = {p_.x,p_.y};
		// complex q = {q_.x,q_.y};
		// complex w = {w_.x,w_.y};
		
		// //Butterfly operation
		// H = add(p,mul(w,q));
		
		pingpong1[x] = float4(HData[x.x].x, HData[x.x].y, 0, 1);
		//GroupMemoryBarrierWithGroupSync();
	}
	// else if(pingpong == 1)
	// {
	// 	float4 data = TwiddleIndices.Load(int3(stage, x.x, 0)).rgba;
	// 	float2 p_ = pingpong1.Load(int3(data.z, x.y, 0)).rg;
	// 	float2 q_ = pingpong1.Load(int3(data.w, x.y, 0)).rg;
	// 	float2 w_ = float2(data.x, data.y);
		
	// 	complex p = {p_.x,p_.y};
	// 	complex q = {q_.x,q_.y};
	// 	complex w = {w_.x,w_.y};
		
	// 	//Butterfly operation
	// 	H = add(p,mul(w,q));
		
	// 	pingpong0[x] = float4(H.real, H.im, 0, 1);
	// }
}

void verticalButterflies(uint2 index)
{
	complex H;
	uint2 x = index;

	float sign = 1.0f;
	//if()

	//int stage = int(g_Constants.Stage_PingPong_Direction_Padding.x);
	//int pingpong = int(g_Constants.Stage_PingPong_Direction_Padding.y);	
	
	// if(pingpong == 0)
	// {
	// 	float4 data = TwiddleIndices.Load(int3(stage, x.y, 0)).rgba;
	// 	float2 p_ = pingpong0.Load(int3(x.x, data.z, 0)).rg;

	// 	float2 q_;
	// 	if(stage == 0)
	// 	{
	// 		if(data.w > 128)
	// 		{
	// 			q_ = pingpong0.Load(int3(x.x, 128 - data.z, 0)).rg;	
	// 			q_.y *= -1;
	// 		}
	// 		else
	// 		{
	// 			q_ = pingpong0.Load(int3(x.x, data.z, 0)).rg;	
	// 		}
			
	// 	}		
	// 	else
	// 	{
	// 		uint strike = 2;
	// 		strike <<= uint(log2(stage));
	// 		uint test_i = (strike - 1) ^ (x.y);
	// 		q_ = pingpong0.Load(int3(x.x, int(test_i), 0)).rg;
	// 	}
	// 	float2 w_ = float2(data.x, data.y);
		
	// 	complex p = {p_.x,p_.y};
	// 	complex q = {q_.x,q_.y};
	// 	complex w = {w_.x,w_.y};
		
	// 	//Butterfly operation
	// 	H = add(p,mul(w,q));
		
	// 	pingpong1[x] = float4(H.real, H.im, 0, 1);
	// }
	//else if(pingpong == 1)
	{
		//float4 data = TwiddleIndices.Load(int3(stage, x.y, 0)).rgba;

		for(int stage = 0; stage < 8; ++stage)
		{
			float2 p_, q_;

			float4 data = TwiddleIndices.Load(int3(stage, x.x, 0)).rgba;

			if(stage == 0)
			{
				p_ = pingpong1.Load(int3(x.y, data.z, 0)).rg;
				q_ = pingpong1.Load(int3(x.y, data.w, 0)).rg;
			}
			else
			{
				p_ = HData[data.z];
				q_ = HData[data.w];

				GroupMemoryBarrierWithGroupSync();
			}			
			float2 w_ = float2(data.x, data.y);
			
			complex p = {p_.x,p_.y};
			complex q = {q_.x,q_.y};
			complex w = {w_.x,w_.y};
			
			//Butterfly operation
			complex r = add(p,mul(w,q));
			HData[x.x] = float2(r.real, r.im);

			GroupMemoryBarrierWithGroupSync();
		}
		// float2 p_ = pingpong1.Load(int3(x.x, data.z, 0)).rg;
		// float2 q_ = pingpong1.Load(int3(x.x, data.w, 0)).rg;
		// float2 w_ = float2(data.x, data.y);
		
		// complex p = {p_.x,p_.y};
		// complex q = {q_.x,q_.y};
		// complex w = {w_.x,w_.y};
		
		//Butterfly operation
		//H = add(p,mul(w,q));
		
		//pingpong0[x] = float4(H.real, H.im, 0, 1);
		pingpong0[x] = float4(HData[x.x].x, HData[x.x].y, 0, 1);
		//GroupMemoryBarrierWithGroupSync();
	}
}

[numthreads(256, 1, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID,
          uint3 DTid : SV_DispatchThreadID)
{
	int direction = int(g_Constants.Stage_PingPong_Direction_Padding.z);
	uint2 ImageIndexInt = DTid.xy;

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
