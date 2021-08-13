//#include "structures.fxh"

static const float M_PI = 3.1415926535897932384626433832795;
static const float g = 9.81;
static const float KM = 370.0;
static const float CM = 0.23;
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
	float noise1 = clamp(g_NoiseTexture0.Load(int3(coord, 0)), 0.001, 1.0);
	float noise2 = clamp(g_NoiseTexture1.Load(int3(coord, 0)), 0.001, 1.0);
	float noise3 = clamp(g_NoiseTexture2.Load(int3(coord, 0)), 0.001, 1.0);
	float noise4 = clamp(g_NoiseTexture3.Load(int3(coord, 0)), 0.001, 1.0);

	float u0 = 2.0*M_PI*noise1;
	float v0 = sqrt(-2.0 * log(noise2));
	float u1 = 2.0*M_PI*noise3;
	float v1 = sqrt(-2.0 * log(noise4));
	
	float4 rnd = float4(v0 * cos(u0), v0 * sin(u0), v1 * cos(u1), v1 * sin(u1));
	
	return rnd;
}

float square (float x)
{
	return x * x;
}

float omega (float k)
{
	return sqrt(g * k * (1.0 + square(k / KM)));
}

float cal_phase(uint2 index, float k, float Time)
{
	float4 gauss_random = GaussRandom(index) * 2.0 * M_PI;
	float deltaPhase = omega(k) * Time;
	float phase = fmod(gauss_random.r + deltaPhase, 2.0 * M_PI);

	return phase;
}

[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID,
          uint3 DTid : SV_DispatchThreadID)
{
	uint2 ImageIndexInt = DTid.xy;

    //uint uiGlobalThreadIdx = GTid.y * uint(THREAD_GROUP_SIZE) + GTid.x;
    // if (uiGlobalThreadIdx >= g_Constants.uiNumParticles)
    //     return;

    float N = g_Constants.N_L_Amplitude_Intensity.x;
    float L = g_Constants.N_L_Amplitude_Intensity.y;
    float Amplitude = g_Constants.N_L_Amplitude_Intensity.z;
    float WindIntensity = g_Constants.N_L_Amplitude_Intensity.w;

    float2 WindDirection = g_Constants.WindDir_LL_Alignment.xy;
    float l = g_Constants.WindDir_LL_Alignment.z;

 //    float2 kl = ImageIndexInt - float(N)/2.0;;
 //    float2 k = float2(2.0 * M_PI * kl.x / L, 2.0 * M_PI * kl.y / L);
 //    float L_ = (WindIntensity * WindIntensity) / g;
 //    float mag = length(k);
 //    mag = max(0.0001, mag);
 //    float MagSq = mag * mag;

 //    //sqrt(Ph(k))/sqrt(2)
	// float h0k = clamp(sqrt((Amplitude/(MagSq*MagSq)) * pow(dot(normalize(k), normalize(WindDirection)), 2) * 
	// 			exp(-(1.0/(MagSq * L_ * L_))) * exp(-MagSq*pow(l,2.0)))/ sqrt(2.0), -40000.0, 40000.0);
	
	// //sqrt(Ph(-k))/sqrt(2)
	// float h0minusk = clamp(sqrt((Amplitude/(MagSq*MagSq)) * pow(dot(normalize(-k), normalize(WindDirection)), 2) * 
	// 				 exp(-(1.0/(MagSq * L_ * L_))) * exp(-MagSq*pow(l,2.0)))/ sqrt(2.0), -40000.0, 40000.0);

	// float4 gauss_random = GaussRandom(ImageIndexInt);

	// buffer_h0k[ImageIndexInt] = float4(gauss_random.xy * h0k, 0, 1);
	// buffer_h0minusk[ImageIndexInt] = float4(gauss_random.zw * h0minusk, 0, 1);

	float2 kl = ImageIndexInt - float(N)/2.0;
	//float2 kl;
	// float half_N = float(N)/2.0;
	// kl.x = (ImageIndexInt.x - 0.5) < half_N ? ImageIndexInt.x : ImageIndexInt.x - half_N;
	// kl.y = (ImageIndexInt.y - 0.5) < half_N ? ImageIndexInt.y : ImageIndexInt.y - half_N;

    float2 WaveVector = float2(2.0 * M_PI * kl.x / L, 2.0 * M_PI * kl.y / L);
    //float L_ = (WindIntensity * WindIntensity) / g;
    float k = length(WaveVector);

    float U10 = WindIntensity;
    float Omega = 0.84;
    float kp = g * square(Omega / U10);
    float c = omega(k) / k;
    float cp = omega(kp) / kp;

    float Lpm = exp(-1.25 * square(kp / k));
    float gamma = 1.7;
    float sigma = 0.08 * (1.0 + 4.0 * pow(Omega, -3.0));
    float Gamma = exp(-square(sqrt(k / kp) - 1.0) / 2.0 * square(sigma));
    float Jp = pow(gamma, Gamma);
    float Fp = Lpm * Jp * exp(-Omega / sqrt(10.0) * (sqrt(k / kp) - 1.0));
    float alphap = 0.006 * sqrt(Omega);
    float Bl = 0.5 * alphap * cp / c * Fp;

    float z0 = 0.000037 * square(U10) / g * pow(U10 / cp, 0.9);
    float uStar = 0.41 * U10 / log(10.0 / z0);
    float alpham = 0.01 * ((uStar < CM) ? (1.0 + log(uStar / CM)) : (1.0 + 3.0 * log(uStar / CM)));
    float Fm = exp(-0.25 * square(k / KM - 1.0));
    float Bh = 0.5 * alpham * CM / c * Fm * Lpm;

    float a0 = log(2.0) / 4.0;
    float am = 0.13 * uStar / CM;
    float Delta = tanh(a0 + 4.0 * pow(c / cp, 2.5) + am * pow(CM / c, 2.5));

    float cosPhi = dot(normalize(WindDirection), normalize(WaveVector));

    float S = (1.0 / (2.0 * M_PI)) * pow(k, -4.0) * (Bl + Bh) * (1.0 + Delta * (2.0 * cosPhi * cosPhi - 1.0));

    float dk = 2.0 * M_PI / L;
    float h = sqrt(S / 2.0) * dk;

    if (WaveVector.x == 0.0 && WaveVector.y == 0.0) 
    {
    	h = 0.0; //no DC term
    }

    //float4 gauss_random = GaussRandom(ImageIndexInt);
    float phase = cal_phase(ImageIndexInt, k, Amplitude);

    buffer_h0k[ImageIndexInt] = float4(h, 0, phase, 1);
}
