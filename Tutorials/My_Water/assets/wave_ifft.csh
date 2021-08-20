static const float PI = 3.1415926;

Texture2D<float2> H0;
RWTexture2D<float4> BufferTmp0;
RWTexture2D<float4> BufferTmp1;
RWTexture2D<float2> DxDz;
RWTexture2D<float2> DyDxz;
RWTexture2D<float2> DyxDyz;
RWTexture2D<float2> DxxDzz;
Texture2D<float4> WavesData;

cbuffer cbIFFTBuffer
{
	float Time;
	float N;
	float Half_N;
	float BitNum;
}

float2 ComplexMult(float2 a, float2 b)
{
	return float2(a.r * b.r - a.g * b.g, a.r * b.g + a.g * b.r);
}

float2 ComplexExp(float2 a)
{
	return float2(cos(a.y), sin(a.y)) * exp(a.x);
}

groupshared float2 DxDzButterFlyCacheData[THREAD_GROUP_SIZE/2];
groupshared float2 DyDxzButterFlyCacheData[THREAD_GROUP_SIZE/2];
groupshared float2 DyxDyzButterFlyCacheData[THREAD_GROUP_SIZE/2];
groupshared float2 DxxDzzButterFlyCacheData[THREAD_GROUP_SIZE/2];

void fft(inout float2 DxDz[2], inout float2 DyDxz[2], inout float2 DyxDyz[2], inout float2 DxxDzz[2], uint x, uint N)
{
	//butterfly	
	bool flag = false;
	float scale = PI * 0.5f; // Pi
							 //stage 0 //cos0 = 1, sin0 = 0
	{
		float2 DxDzt = DxDz[1];
		DxDz[1] = DxDz[0] - DxDzt;
		DxDz[0] = DxDz[0] + DxDzt;

		float2 DyDxzt = DyDxz[1];
		DyDxz[1] = DyDxz[0] - DyDxzt;
		DyDxz[0] = DyDxz[0] + DyDxzt;

		float2 DyxDyzt = DyxDyz[1];
		DyxDyz[1] = DyxDyz[0] - DyxDyzt;
		DyxDyz[0] = DyxDyz[0] + DyxDyzt;

		float2 DxxDzzt = DxxDzz[1];
		DxxDzz[1] = DxxDzz[0] - DxxDzzt;
		DxxDzz[0] = DxxDzz[0] + DxxDzzt;

		flag = x & 1;
		if (flag)
		{
			DxDzButterFlyCacheData[x] = DxDz[0];
			DyDxzButterFlyCacheData[x] = DyDxz[0];
			DyxDyzButterFlyCacheData[x] = DyxDyz[0];
			DxxDzzButterFlyCacheData[x] = DxxDzz[0];
		}
		else
		{
			DxDzButterFlyCacheData[x] = DxDz[1];
			DyDxzButterFlyCacheData[x] = DyDxz[1];
			DyxDyzButterFlyCacheData[x] = DyxDyz[1];
			DxxDzzButterFlyCacheData[x] = DxxDzz[1];
		}

		GroupMemoryBarrierWithGroupSync();
	}

	uint stage_id = 1;
	for (uint stage = 2; stage < N; stage <<= 1, scale *= 0.5)
	{
		//float4 data = PrecomputedData[uint2(stage_id, x)];
		uint i = x ^ (stage - 1); //pair index
		uint j = x & (stage - 1); //curr n index

		if (flag)
		{
			DxDz[0] = DxDzButterFlyCacheData[i];
			DyDxz[0] = DyDxzButterFlyCacheData[i];
			DyxDyz[0] = DyxDyzButterFlyCacheData[i];
			DxxDzz[0] = DxxDzzButterFlyCacheData[i];
		}
		else
		{
			DxDz[1] = DxDzButterFlyCacheData[i];
			DyDxz[1] = DyDxzButterFlyCacheData[i];
			DyxDyz[1] = DyxDyzButterFlyCacheData[i];
			DxxDzz[1] = DxxDzzButterFlyCacheData[i];
		}

		float e_sin, e_cos;
		sincos(j * scale, e_sin, e_cos);
		float2 omega_j = float2(e_cos, e_sin);		

		float2 DxDzt = ComplexMult(omega_j, DxDz[1]);
		float2 DyDxzt = ComplexMult(omega_j, DyDxz[1]);
		float2 DyxDyzt = ComplexMult(omega_j, DyxDyz[1]);
		float2 DxxDzzt = ComplexMult(omega_j, DxxDzz[1]);

		DxDz[1] = DxDz[0] - DxDzt;
		DxDz[0] = DxDz[0] + DxDzt;

		DyDxz[1] = DyDxz[0] - DyDxzt;
		DyDxz[0] = DyDxz[0] + DyDxzt;

		DyxDyz[1] = DyxDyz[0] - DyxDyzt;
		DyxDyz[0] = DyxDyz[0] + DyxDyzt;

		DxxDzz[1] = DxxDzz[0] - DxxDzzt;
		DxxDzz[0] = DxxDzz[0] + DxxDzzt;

		flag = x & stage;

		if (flag)
		{
			DxDzButterFlyCacheData[i] = DxDz[0];
			DyDxzButterFlyCacheData[i] = DyDxz[0];
			DyxDyzButterFlyCacheData[i] = DyxDyz[0];
			DxxDzzButterFlyCacheData[i] = DxxDzz[0];
		}
		else
		{
			DxDzButterFlyCacheData[i] = DxDz[1];
			DyDxzButterFlyCacheData[i] = DyDxz[1];
			DyxDyzButterFlyCacheData[i] = DyxDyz[1];
			DxxDzzButterFlyCacheData[i] = DxxDzz[1];
		}

		GroupMemoryBarrierWithGroupSync();
	}
}

//groupshared float4 FullButterFlyCacheData0[256];
//groupshared float4 FullButterFlyCacheData1[256];

void CalculateDDData(uint2 id, out float2 dxdz, out float2 dydxz, out float2 dyxdyz, out float2 dxxdzz)
{
	float4 wave = WavesData[id.xy];
	float phase = wave.w * Time;
	float2 exponent = float2(cos(phase), sin(phase));
	float2 h = ComplexMult(H0[id.xy].xy, exponent)
		+ ComplexMult(H0[uint2(N, N) - id.xy].xy * float2(1, -1), float2(exponent.x, -exponent.y));
	float2 ih = float2(-h.y, h.x);

	float2 displacementX = ih * wave.x * wave.y;
	float2 displacementY = h;
	float2 displacementZ = ih * wave.z * wave.y;

	float2 displacementX_dx = -h * wave.x * wave.x * wave.y;
	float2 displacementY_dx = ih * wave.x;
	float2 displacementZ_dx = -h * wave.x * wave.z * wave.y;

	float2 displacementY_dz = ih * wave.z;
	float2 displacementZ_dz = -h * wave.z * wave.z * wave.y;

	dxdz = float2(displacementX.x - displacementZ.y, displacementX.y + displacementZ.x);
	dydxz = float2(displacementY.x - displacementZ_dx.y, displacementY.y + displacementZ_dx.x);
	dyxdyz = float2(displacementY_dx.x - displacementY_dz.y, displacementY_dx.y + displacementY_dz.x);
	dxxdzz = float2(displacementX_dx.x - displacementZ_dz.y, displacementX_dx.y + displacementZ_dz.x);
}


[numthreads(THREAD_GROUP_SIZE/2, 1, 1)]
void RowInverseFFT(uint3 id : SV_DispatchThreadID)
{
	//const uint Half_N = N / 2;
	const uint BitNum = log2(N);

	uint Column = id.x * 2;
	uint Row = id.y;
	uint ReverseColumnIdx = reversebits(Column) >> (32 - BitNum);
	uint PairColumnIdx = ReverseColumnIdx + Half_N;

	uint2 Idx0 = uint2(ReverseColumnIdx, Row);
	uint2 Idx1 = uint2(PairColumnIdx, Row);

	//Calculate H
	float2 dxdz[2];
	float2 dydxz[2];
	float2 dyxdyz[2];
	float2 dxxdzz[2];
	CalculateDDData(Idx0, dxdz[0], dydxz[0], dyxdyz[0], dxxdzz[0]);
	CalculateDDData(Idx1, dxdz[1], dydxz[1], dyxdyz[1], dxxdzz[1]);

	/*float2 h[2];
	h[0] = Buffer0[Idx0];
	h[1] = Buffer0[Idx1];*/

	//loop
	fft(dxdz, dydxz, dyxdyz, dxxdzz, id.x, N);		

	uint2 BufferIdx = id.xy;
	BufferTmp0[BufferIdx] = float4(dxdz[0], dydxz[0]);
	BufferTmp0[BufferIdx + uint2(Half_N, 0)] = float4(dxdz[1], dydxz[1]);

	BufferTmp1[BufferIdx] = float4(dyxdyz[0], dxxdzz[0]);
	BufferTmp1[BufferIdx + uint2(Half_N, 0)] = float4(dyxdyz[1], dxxdzz[1]);
}

[numthreads(THREAD_GROUP_SIZE/2, 1, 1)]
void ColumnInverseFFT(uint3 id : SV_DispatchThreadID)
{
	//const uint Half_N = N / 2;
	const uint BitNum = log2(N);

	uint Row = id.x * 2;
	uint Column = id.y;
	uint ReverseRowIdx = reversebits(Row) >> (32 - BitNum);
	//uint PairRowIdx = Half_N - ReverseRowIdx;
	uint PairRowIdx = ReverseRowIdx + Half_N;

	uint2 Idx0 = uint2(Column, ReverseRowIdx);
	uint2 Idx1 = uint2(Column, PairRowIdx);

	float2 dxdz[2];
	float2 dydxz[2];	
	dxdz[0] = BufferTmp0[Idx0].xy;
	dydxz[0] = BufferTmp0[Idx0].zw;
	dxdz[1] = BufferTmp0[Idx1].xy;
	dydxz[1] = BufferTmp0[Idx1].zw;
	

	float2 dyxdyz[2];
	float2 dxxdzz[2];
	dyxdyz[0] = BufferTmp1[Idx0].xy;
	dxxdzz[0] = BufferTmp1[Idx0].zw;
	dyxdyz[1] = BufferTmp1[Idx1].xy;
	dxxdzz[1] = BufferTmp1[Idx1].zw;

	//test
	//uint2 TestPartIdx = uint2(Column, N - PairRowIdx);
	//h[1] = Buffer1[TestPartIdx];
	//h[1].y = -h[1].y;

	//loop
	fft(dxdz, dydxz, dyxdyz, dxxdzz, id.x, N);

	uint2 BufferIdx = uint2(Column, id.x);
	uint2 PairBufferIdx = BufferIdx + uint2(0, Half_N);

	float PermuteIdx = (1.0 - 2.0 * ((BufferIdx.x + BufferIdx.y) % 2));
	float PermutePair = (1.0 - 2.0 * ((PairBufferIdx.x + PairBufferIdx.y) % 2));

	DxDz[BufferIdx] = dxdz[0] * PermuteIdx;
	DxDz[BufferIdx + uint2(0, Half_N)] = dxdz[1] * PermutePair;

	DyDxz[BufferIdx] = dydxz[0] * PermuteIdx;
	DyDxz[BufferIdx + uint2(0, Half_N)] = dydxz[1] * PermutePair;

	DyxDyz[BufferIdx] = dyxdyz[0] * PermuteIdx;
	DyxDyz[BufferIdx + uint2(0, Half_N)] = dyxdyz[1] * PermutePair;

	DxxDzz[BufferIdx] = dxxdzz[0] * PermuteIdx;
	DxxDzz[BufferIdx + uint2(0, Half_N)] = dxxdzz[1] * PermutePair;
}
