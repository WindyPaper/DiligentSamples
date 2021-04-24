//#include "structures.fxh"

static const float M_PI = 3.1415926535897932384626433832795;
static const float g = 9.81;
//noise texture, can we conbine to a texture array?
Texture2D    g_NoiseTexture0;
//SamplerState g_NoiseTexture1_sampler; // By convention, texture samplers must use the '_sampler' suffix
Texture2D    g_NoiseTexture1;
//SamplerState g_NoiseTexture2_sampler;
Texture2D    g_NoiseTexture2;
//SamplerState g_NoiseTexture3_sampler;
Texture2D    g_NoiseTexture3;
//SamplerState g_NoiseTexture4_sampler;

RWTexture2D<float4> buffer_h0k;
RWTexture2D<float4> buffer_h0minusk;

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

float4 GaussRandom(uint2 coord)
{
	float noise1 = saturate(g_NoiseTexture0.Load(int3(coord, 0)));
	float noise2 = saturate(g_NoiseTexture1.Load(int3(coord, 0)));
	float noise3 = saturate(g_NoiseTexture2.Load(int3(coord, 0)));
	float noise4 = saturate(g_NoiseTexture3.Load(int3(coord, 0)));

	float u0 = 2.0*M_PI*noise1;
	float v0 = sqrt(-2.0 * log(noise2));
	float u1 = 2.0*M_PI*noise3;
	float v1 = sqrt(-2.0 * log(noise4));
	
	float4 rnd = float4(v0 * cos(u0), v0 * sin(u0), v1 * cos(u1), v1 * sin(u1));
	
	return rnd;
}

[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID)
{
	uint2 ImageIndexInt = GTid.xy;

    uint uiGlobalThreadIdx = GTid.y * uint(THREAD_GROUP_SIZE) + GTid.x;
    // if (uiGlobalThreadIdx >= g_Constants.uiNumParticles)
    //     return;

    float N = g_Constants.N_L_Amplitude_Intensity.x;
    float L = g_Constants.N_L_Amplitude_Intensity.y;
    float Amplitude = g_Constants.N_L_Amplitude_Intensity.z;
    float WindIntensity = g_Constants.N_L_Amplitude_Intensity.w;

    float2 WindDirection = g_Constants.WindDir_LL_Alignment.xy;
    float l = g_Constants.WindDir_LL_Alignment.z;

    float2 kl = ImageIndexInt - float(N)/2.0;;
    float2 k = float2(2.0 * M_PI * kl.x / L, 2.0 * M_PI * kl.y / L);
    float L_ = (WindIntensity * WindIntensity) / g;
    float mag = length(k);
    mag = max(0.0001, mag);
    float MagSq = mag * mag;

    //sqrt(Ph(k))/sqrt(2)
	float h0k = clamp(sqrt((Amplitude/(MagSq*MagSq)) * pow(dot(normalize(k), normalize(WindDirection)), 2) * 
				exp(-(1.0/(MagSq * L_ * L_))) * exp(-MagSq*pow(l,2.0)))/ sqrt(2.0), -4000.0, 4000.0);
	
	//sqrt(Ph(-k))/sqrt(2)
	float h0minusk = clamp(sqrt((Amplitude/(MagSq*MagSq)) * pow(dot(normalize(-k), normalize(WindDirection)), 2) * 
					 exp(-(1.0/(MagSq * L_ * L_))) * exp(-MagSq*pow(l,2.0)))/ sqrt(2.0), -4000.0, 4000.0);

	float4 gauss_random = GaussRandom(ImageIndexInt);

	buffer_h0k[ImageIndexInt] = float4(gauss_random.xy * h0k, 0, 1);
	buffer_h0minusk[ImageIndexInt] = float4(gauss_random.zw * h0minusk, 0, 1);
}
