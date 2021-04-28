//#include "structures.fxh"
#include "complex.fxh"

static const float M_PI = 3.1415926535897932384626433832795;
static const float g = 9.81;

RWTexture2D<float4> TwiddleIndices;

Buffer<int> bit_reversed;

struct WaterFFTHKTUniform
{
	float4 N_padding; //pack
};

cbuffer Constants
{
    WaterFFTHKTUniform g_Constants;
};

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 64
#endif

[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID,
          uint3 DTid : SV_DispatchThreadID)
{
	float N = g_Constants.N_padding.x;

	uint2 ImageIndexInt = DTid.xy;

    // uint uiGlobalThreadIdx = GTid.y * uint(THREAD_GROUP_SIZE) + GTid.x;
    // if (uiGlobalThreadIdx >= g_Constants.uiNumParticles)
    //     return;    
    float k = fmod(ImageIndexInt.y * (float(N)/ pow(2,ImageIndexInt.x+1)), N);
	complex twiddle;
	twiddle.real = cos(2.0*M_PI*k/float(N));
	twiddle.im = sin(2.0*M_PI*k/float(N));
	
	int butterflyspan = int(pow(2, ImageIndexInt.x));
	
	int butterflywing;
	
	if (fmod(ImageIndexInt.y, pow(2, ImageIndexInt.x + 1)) < pow(2, ImageIndexInt.x))
		butterflywing = 1;
	else butterflywing = 0;

	int CurrNIndex = ImageIndexInt.y;

	// first stage, bit reversed indices
	if (ImageIndexInt.x == 0) {
		// top butterfly wing
		if (butterflywing == 1)
			TwiddleIndices[ImageIndexInt] = float4(twiddle.real, twiddle.im, bit_reversed[int(CurrNIndex)], bit_reversed[int(CurrNIndex + 1)]);
		// bot butterfly wing
		else	
			TwiddleIndices[ImageIndexInt] = float4(twiddle.real, twiddle.im, bit_reversed[int(CurrNIndex - 1)], bit_reversed[int(CurrNIndex)]);
	}
	// second to log2(N) stage
	else {
		// top butterfly wing
		if (butterflywing == 1)
			TwiddleIndices[ImageIndexInt] = float4(twiddle.real, twiddle.im, CurrNIndex, CurrNIndex + butterflyspan);
		// bot butterfly wing
		else
			TwiddleIndices[ImageIndexInt] = float4(twiddle.real, twiddle.im, CurrNIndex - butterflyspan, ImageIndexInt.y);
	}
}
