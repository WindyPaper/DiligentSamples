//#include "structures.fxh"
#include "complex.fxh"

static const float M_PI = 3.1415926535897932384626433832795;
static const float g = 9.81;

Texture2D h0k_tex;
Texture2D h0minusk_tex;

RWBuffer<float4> hkt_dx;
RWBuffer<float4> hkt_dy;
RWBuffer<float4> hkt_dz;

struct WaterFFTHKTUniform
{
	float4 N_L_Time_padding; //pack
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
          uint3 GTid : SV_GroupThreadID)
{
	float N = g_Constants.N_L_Time_padding.x;
    float L = g_Constants.N_L_Time_padding.y;
    float time = g_Constants.N_L_Time_padding.z;

	uint2 ImageIndexInt = GTid.xy;

    uint uiGlobalThreadIdx = GTid.y * uint(THREAD_GROUP_SIZE) + GTid.x;
    // if (uiGlobalThreadIdx >= g_Constants.uiNumParticles)
    //     return;    
    float2 kl = ImageIndexInt - float(N)/2.0;;    

    float2 k = float2(2.0 * M_PI * kl.x / L, 2.0 * M_PI * kl.y / L);
    
    float magnitude = length(k);
	if (magnitude < 0.00001) magnitude = 0.00001;
	
	float w = sqrt(9.81 * magnitude);
	
	float4 h0tex_data = h0k_tex.Load(int(ImageIndexInt, 0));
	float4 h0minusk_data = h0minusk_tex.Load(int(ImageIndexInt, 0));

	complex fourier_amp = complex(h0tex_data.r, h0minusk_data.g);	
	complex fourier_amp_conj = conj(complex(h0minusk_data.r, h0minusk_data.g));
		
	float cosinus = cos(w*time);
	float sinus   = sin(w*time);
		
	// euler formula
	complex exp_iwt = complex(cosinus, sinus);
	complex exp_iwt_inv = complex(cosinus, -sinus);
	
	// dy
	complex h_k_t_dy = add(mul(fourier_amp, exp_iwt), (mul(fourier_amp_conj, exp_iwt_inv)));
	
	// dx
	complex dx = complex(0.0,-k.x/magnitude);
	complex h_k_t_dx = mul(dx, h_k_t_dy);
	
	// dz
	complex dy = complex(0.0,-k.y/magnitude);
	complex h_k_t_dz = mul(dy, h_k_t_dy);
		
	hkt_dy[uiGlobalThreadIdx] = float4(h_k_t_dy.real, h_k_t_dy.im, 0, 1);
	hkt_dx[uiGlobalThreadIdx] = float4(h_k_t_dx.real, h_k_t_dx.im, 0, 1);
	hkt_dz[uiGlobalThreadIdx] = float4(h_k_t_dz.real, h_k_t_dz.im, 0, 1);
}
