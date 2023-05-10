#ifndef VERTEX_AO_SAMPLE_NUM
#   define VERTEX_AO_SAMPLE_NUM 256
#endif

cbuffer GenVertexAORaysUniformData
{
    int num_vertex;  
}

struct GenAORayData
{
    float3 o;
    float3 dir;
};

RWStructuredBuffer<GenAORayData> out_ao_ray_datas;

uint CMJPermute(uint i, uint l, uint p)
{
    uint w = l - 1;
    w |= w >> 1;
    w |= w >> 2;
    w |= w >> 4;
    w |= w >> 8;
    w |= w >> 16;
    do
    {
        i ^= p; i *= 0xe170893d;
        i ^= p >> 16;
        i ^= (i & w) >> 4;
        i ^= p >> 8; i *= 0x0929eb3f;
        i ^= p >> 23;
        i ^= (i & w) >> 1; i *= 1 | p >> 27;
        i *= 0x6935fa69;
        i ^= (i & w) >> 11; i *= 0x74dcb303;
        i ^= (i & w) >> 2; i *= 0x9e501cc3;
        i ^= (i & w) >> 2; i *= 0xc860a3df;
        i &= w;
        i ^= i >> 5;
    }
    while (i >= l);
    return (i + p) % l;
}

float CMJRandFloat(uint i, uint p)
{
    i ^= p;
    i ^= i >> 17;
    i ^= i >> 10; i *= 0xb36534e5;
    i ^= i >> 12;
    i ^= i >> 21; i *= 0x93fc4795;
    i ^= 0xdf6e307f;
    i ^= i >> 17; i *= 1 | p >> 18;
    return i * (1.0f / 4294967808.0f);
}

 // Returns a 2D sample from a particular pattern using correlated multi-jittered sampling [Kensler 2013]
float2 SampleCMJ2D(uint sampleIdx, uint numSamplesX, uint numSamplesY, uint pattern)
{
    uint N = numSamplesX * numSamplesY;
    sampleIdx = CMJPermute(sampleIdx, N, pattern * 0x51633e2d);
    uint sx = CMJPermute(sampleIdx % numSamplesX, numSamplesX, pattern * 0x68bc21eb);
    uint sy = CMJPermute(sampleIdx / numSamplesX, numSamplesY, pattern * 0x02e5be93);
    float jx = CMJRandFloat(sampleIdx, pattern * 0x967a889b);
    float jy = CMJRandFloat(sampleIdx, pattern * 0x368cc8b7);
    return float2((sx + (sy + jx) / numSamplesY) / numSamplesX, (sampleIdx + jy) / N);
}

float2 SamplePoint(in uint curr_sample_idx, in uint pixelIdx, inout uint setIdx)
{
    const uint permutation = setIdx * VERTEX_AO_SAMPLE_NUM + pixelIdx;
    setIdx += 1;
    const float sqrt_sample_num = sqrt(VERTEX_AO_SAMPLE_NUM);
    return SampleCMJ2D(curr_sample_idx, sqrt_sample_num, sqrt_sample_num, permutation);
}

float2 ConcentricSampleDisk(const float2 &u)
{
	float2 uOffset = 2.f * u - float2(1, 1);
	if (uOffset.x == 0 && uOffset.y == 0) return float2(0, 0);

	float theta, r;
	if (uOffset.x * uOffset.x > uOffset.y * uOffset.y) {
		r = uOffset.x;
		theta = (3.1415926f / 4.0f) * (uOffset.y / uOffset.x);
	}
	else {
		r = uOffset.y;
		theta = (3.1415926f / 2.0f) - (3.1415926f / 4.0f) * (uOffset.x / uOffset.y);
	}
	return r * float2(cosf(theta), sinf(theta));
}

[numthreads(VERTEX_AO_SAMPLE_NUM, 1, 1)]
void GenVertexAORaysMain(uint3 gid : SV_GroupID, uint3 id : SV_DispatchThreadID, uint local_grp_idx : SV_GroupIndex)
{
    uint vertex_ray_id = id.x;

    if(vertex_ray_id > (num_vertex - 1))
    {
        return;
    }

    float2 sample_uv = SamplePoint(vertex_ray_id, gid.x, local_grp_idx.x);
}