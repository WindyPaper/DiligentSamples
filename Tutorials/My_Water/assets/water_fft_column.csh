//#include "structures.fxh"

static const float M_PI = 3.1415926535897932384626433832795;
//static const float g = 9.81;

Texture2D<float2> HtInput;
Texture2D<float4> DtInput;

RWTexture2D<float4> displacement;

struct WaterFastFFT
{
	float4 N_ChoppyScale_NBitNum_Time;
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

float2 MultiplyComplex(float2 a, float2 b)
{
	return float2(a[0] * b[0] - a[1] * b[1], a[1] * b[0] + a[0] * b[1]);
}

void fft(inout float2 h[2], inout float2 x[2], inout float2 z[2], uint thread_id, uint N)
{
	//butterfly
	
	//stage 0 //cos0 = 1, sin0 = 0
	float2 dht = h[1];
	float2 dxt = x[1];
	float2 dzt = z[1];

	h[1] = h[0] - dht;
	h[0] = h[0] + dht;
	x[1] = x[0] - dxt;
	x[0] = x[0] + dxt;
	z[1] = z[0] - dzt;
	z[0] = z[0] + dzt;

	bool flag = thread_id & 1;
	float scale = M_PI * 0.5f; // Pi

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

		dht = MultiplyComplex(omega_j, h[1]);
		dxt = MultiplyComplex(omega_j, x[1]);
		dzt = MultiplyComplex(omega_j, z[1]);

		h[1] = h[0] - dht;
		h[0] = h[0] + dht;
		x[1] = x[0] - dxt;
		x[0] = x[0] + dxt;
		z[1] = z[0] - dzt;
		z[0] = z[0] + dzt;

		flag = thread_id & stage;

		if(flag)
		{
			hData[i] = h[0];
			xData[i] = x[0];
			zData[i] = z[0];
		}
		else
		{
			hData[i] = h[1];
			xData[i] = x[1];
			zData[i] = z[1];
		}

		GroupMemoryBarrierWithGroupSync();
	}
}

[numthreads(THREAD_GROUP_SIZE/2, 1, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID,
          uint3 DTid : SV_DispatchThreadID)
{
	//uint2 ImageIndexInt = DTid.xy;
	uint row = DTid.x;
	uint column = DTid.y;

	float2 rowh[2], rowx[2], rowz[2];

	uint N = g_Constants.N_ChoppyScale_NBitNum_Time.x;
	uint half_N = N / 2.0;
	float choppy_scale = g_Constants.N_ChoppyScale_NBitNum_Time.y;
	uint NBitNum = g_Constants.N_ChoppyScale_NBitNum_Time.z;
	
	uint reverse_bit_row = reversebits(row * 2) >> NBitNum;

	//uint row_index = N * reverse_bit_row + column;
	//uint next_row_index = (row_index + half_N * N);
	//uint next_row_index = (half_N - reverse_bit_row) * N + column;

	uint row_index = reverse_bit_row;
	uint next_row_index = (half_N - reverse_bit_row); // (N - (rowindex + half_N)) = half_N - reverse_bit_row

	//bitreverse(n)
	rowh[0] = HtInput.Load(int3(column, row_index, 0)).xy;	

	float4 dt_data0 = DtInput.Load(int3(column, row_index, 0));
	rowx[0] = dt_data0.xy;
	rowz[0] = dt_data0.zw;

	//bitreverse(n+1) conj
	rowh[1] = HtInput.Load(int3(column, next_row_index, 0)).xy;
	rowh[1].y *= -1;
	float4 dt_data1 = DtInput.Load(int3(column, next_row_index, 0));
	rowx[1] = dt_data1.xy;
	rowx[1].y *= -1;
	rowz[1] = dt_data1.zw;
	rowz[1].y *= -1;
	
	fft(rowh, rowx, rowz, row, N);

	float sign = (row + column) & 0x1 ? -1.0f : 1.0f;	
	float scale = 1 * sign;

	displacement[uint2(column, row)] = float4(rowx[0].x * scale, rowh[0].x * sign, rowz[0].x * scale, 0.0f);
	displacement[uint2(column, row + half_N)] = float4(rowx[1].x * scale, rowh[1].x * sign, rowz[1].x * scale, 0.0f);
    //uint uiGlobalThreadIdx = GTid.y * uint(THREAD_GROUP_SIZE) + GTid.x;
    // if (uiGlobalThreadIdx >= g_Constants.uiNumParticles)
    //     return;

    
}
