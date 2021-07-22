//#include "structures.fxh"

static const float M_PI = 3.1415926535897932384626433832795;
static const float g = 9.81;

Texture2D H0;

RWTexture2D<float2> HtOutput;
RWTexture2D<float4> DtOutput;

struct WaterFastFFT
{
	float4 N_H0DataLength_NBitNum_Time;
};

cbuffer Constants
{
    WaterFastFFT g_Constants;
};

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 64
#endif

//share datas for conmunicate between the threads
groupshared float2 hData[THREAD_GROUP_SIZE/2];
groupshared float2 xData[THREAD_GROUP_SIZE/2];
groupshared float2 zData[THREAD_GROUP_SIZE/2];

float Omega(uint2 coord, uint half_N, inout float2 k, inout float magnitude)
{
	uint2 kl = coord - uint2(half_N, half_N);

	float L = 1000;
	k = float2(2.0 * M_PI * kl.x / L, 2.0 * M_PI * kl.y / L);

	magnitude = length(k);
	if (magnitude < 0.00001) magnitude = 0.00001;
	
	float w = (9.81 * magnitude);
	return w;
}

float2 MultiplyComplex(float2 a, float2 b)
{
	return float2(a[0] * b[0] - a[1] * b[1], a[1] * b[0] + a[0] * b[1]);
}

float2 MultiplyI(float2 c)
{
	return float2(-c[1], c[0]);
}

void fft(inout float2 h[2], inout float2 x[2], inout float2 z[2], uint thread_id, uint N)
{
	//butterfly
	
	bool flag = thread_id & 1;
	float scale = M_PI * 0.5f; // Pi

	//stage 0 //cos0 = 1, sin0 = 0
	{
		float2 dht = h[1];
		float2 dxt = x[1];
		float2 dzt = z[1];

		h[1] = h[0] - dht;
		h[0] = h[0] + dht;
		x[1] = x[0] - dxt;
		x[0] = x[0] + dxt;
		z[1] = z[0] - dzt;
		z[0] = z[0] + dzt;


		if(flag)
		{
			hData[thread_id] = h[0];
			xData[thread_id] = x[0];
			zData[thread_id] = z[0];
		}
		else
		{
			hData[thread_id] = h[1];
			xData[thread_id] = x[1];
			zData[thread_id] = z[1];
		}

		GroupMemoryBarrier();
	}

	for(uint stage = 2; stage < N; stage <<= 1, scale *= 0.5)
	{
		uint i = thread_id ^ (stage - 1); //pair index
		uint j = thread_id & (stage - 1); //curr n index

		if(flag)
		{
			h[0] = hData[i];
			x[0] = xData[i];
			z[0] = zData[i];
		}
		else
		{
			h[1] = hData[i];
			x[1] = xData[i];
			z[1] = zData[i];
		}

		float e_sin, e_cos;
		sincos(j * scale, e_sin, e_cos);
		float2 omega_j = float2(e_cos, e_sin);

		float2 dht = MultiplyComplex(omega_j, h[1]);
		float2 dxt = MultiplyComplex(omega_j, x[1]);
		float2 dzt = MultiplyComplex(omega_j, z[1]);

		h[1] = h[0] - dht;
		h[0] = h[0] + dht;
		x[1] = x[0] - dxt;
		x[0] = x[0] + dxt;
		z[1] = z[0] - dzt;
		z[0] = z[0] + dzt;

		flag = thread_id & stage;

		if(flag)
		{
			hData[thread_id] = h[0];
			xData[thread_id] = x[0];
			zData[thread_id] = z[0];
		}
		else
		{
			hData[thread_id] = h[1];
			xData[thread_id] = x[1];
			zData[thread_id] = z[1];
		}

		GroupMemoryBarrierWithGroupSync();
	}
}

[numthreads(THREAD_GROUP_SIZE/2, 1, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID,
          uint3 DTid : SV_DispatchThreadID)
{
	uint2 ImageIndexInt = DTid.xy;

	uint N = g_Constants.N_H0DataLength_NBitNum_Time.x;
	uint half_N = N / 2;
	uint NH0DataLength = g_Constants.N_H0DataLength_NBitNum_Time.y;
	uint NBitNum = g_Constants.N_H0DataLength_NBitNum_Time.z;
	float Time = g_Constants.N_H0DataLength_NBitNum_Time.w;	

	//uint stage_num = log2(N);

	uint base_x = ImageIndexInt.x * 2;
	uint y = ImageIndexInt.y;

	uint reverse_bit_column = reversebits(base_x) >> NBitNum;
	uint next_index_column = reverse_bit_column + half_N;

	float4 h0;
	float4 h0_star;

	int2 first_stage_index0 = int2(reverse_bit_column, y);
	int2 first_stage_index1 = int2(next_index_column, y);
	h0.xy = H0.Load(int3(first_stage_index0, 0)).xy; //pair 0
	h0.zw = H0.Load(int3(first_stage_index1, 0)).xy; //pair 1

	int2 h0_star_index0 = int2(N, N) - first_stage_index0;
	int2 h0_star_index1 = int2(N, N) - first_stage_index1;
	h0_star.xy = H0.Load(int3(h0_star_index0, 0)).xy * float2(1.0, -1.0); // conj
	h0_star.zw = H0.Load(int3(h0_star_index1, 0)).xy * float2(1.0, -1.0); // conj

	float2 ht[2], xt[2], zt[2];

	//omega
	float2 kv[2];
	float magnitudev[2];
	float omega0 = Omega(first_stage_index0, half_N, kv[0], magnitudev[0]);
	float omega1 = Omega(first_stage_index1, half_N, kv[1], magnitudev[1]);

	//dispersion
	float T = 200.0f;
	float w_zero = 2.0f * M_PI / T;
	float dispersion0 = floor(sqrt(omega0) / w_zero) * w_zero * Time;
	float dispersion1 = floor(sqrt(omega1) / w_zero) * w_zero * Time;

	float2 sin_omega, cos_omega;
	sincos(float2(dispersion0, dispersion1), sin_omega, cos_omega);
	float2 phase0 = float2(cos_omega.x, sin_omega.x);
	float2 phase1 = float2(cos_omega.y, sin_omega.y);

	ht[0] = MultiplyComplex(h0.xy, phase0) + MultiplyComplex(h0_star.xy, float2(phase0.x, -phase0.y));
	ht[1] = MultiplyComplex(h0.zw, phase1) + MultiplyComplex(h0_star.zw, float2(phase1.x, -phase1.y));

	xt[0] = -MultiplyI(ht[0] * (kv[0].x / magnitudev[0]));
	zt[0] = -MultiplyI(ht[0] * (kv[0].y / magnitudev[0]));

	xt[1] = -MultiplyI(ht[1] * (kv[1].x / magnitudev[1]));
	zt[1] = -MultiplyI(ht[1] * (kv[1].y / magnitudev[1]));

	fft(ht, xt, zt, ImageIndexInt.x, N);

    //uint uiGlobalThreadIdx = GTid.y * uint(THREAD_GROUP_SIZE) + GTid.x;
    // if (uiGlobalThreadIdx >= g_Constants.uiNumParticles)
    //     return;
	HtOutput[ImageIndexInt] = ht[0];
	HtOutput[ImageIndexInt + uint2(half_N, 0)] = ht[1];

	DtOutput[ImageIndexInt] = float4(xt[0], zt[0]);
	DtOutput[ImageIndexInt + uint2(half_N, 0)] = float4(xt[1], zt[1]);    
}
