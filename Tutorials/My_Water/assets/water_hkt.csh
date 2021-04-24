//#include "structures.fxh"
#include "complex.fxh"

static const float M_PI = 3.1415926535897932384626433832795;
static const float g = 9.81;

Texture2D h0k_buffer;
Texture2D h0minusk_buffer;

RWTexture2D<float4> hkt_dx;
RWTexture2D<float4> hkt_dy;
RWTexture2D<float4> hkt_dz;

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
          uint3 GTid : SV_GroupThreadID,
          uint3 DTid : SV_DispatchThreadID)
{
	float N = g_Constants.N_L_Time_padding.x;
    float L = g_Constants.N_L_Time_padding.y;
    float time = g_Constants.N_L_Time_padding.z;

	uint2 ImageIndexInt = DTid.xy; //GTid.xy;

    //uint uiGlobalThreadIdx = GTid.y * uint(THREAD_GROUP_SIZE) + GTid.x;
    // if (uiGlobalThreadIdx >= g_Constants.uiNumParticles)
    //     return;    
    float2 kl = ImageIndexInt - float(N)/2.0;;    

    float2 k = float2(2.0 * M_PI * kl.x / L, 2.0 * M_PI * kl.y / L);
    
    float magnitude = length(k);
	if (magnitude < 0.00001) magnitude = 0.00001;
	
	float w = sqrt(9.81 * magnitude);
	
	float4 h0tex_data = h0k_buffer.Load(int3(ImageIndexInt, 0));
	float4 h0minusk_data = h0minusk_buffer.Load(int3(ImageIndexInt, 0));

	complex fourier_amp;
	fourier_amp.real = h0tex_data.r;
	fourier_amp.im = h0tex_data.g;//complex(h0tex_data.r, h0minusk_data.g);	
	complex fourier_amp_conj;
	fourier_amp_conj.real = h0minusk_data.r;
	fourier_amp_conj.im = h0minusk_data.g; //= conj(complex(h0minusk_data.r, h0minusk_data.g));
	fourier_amp_conj = conj(fourier_amp_conj);
		
	float cosinus = cos(w*time);
	float sinus   = sin(w*time);
		
	// euler formula
	complex exp_iwt = { cosinus, sinus };
	complex exp_iwt_inv = {cosinus, -sinus };
	
	// dy
	complex h_k_t_dy = add(mul(fourier_amp, exp_iwt), (mul(fourier_amp_conj, exp_iwt_inv)));
	
	// dx
	complex dx = { 0.0,-k.x/magnitude };
	complex h_k_t_dx = mul(dx, h_k_t_dy);
	
	// dz
	complex dy = { 0.0,-k.y/magnitude };
	complex h_k_t_dz = mul(dy, h_k_t_dy);
		
	hkt_dy[ImageIndexInt] = float4(h_k_t_dy.real, h_k_t_dy.im, 0, 1);
	hkt_dx[ImageIndexInt] = float4(h_k_t_dx.real, h_k_t_dx.im, 0, 1);
	hkt_dz[ImageIndexInt] = float4(h_k_t_dz.real, h_k_t_dz.im, 0, 1);
}
