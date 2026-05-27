// Copyright Epic Games, Inc. All Rights Reserved.

// #include "../Common.ush"
// #include "../CommonViewUniformBuffer.ush"
// #include "../SceneTextureParameters.ush"
// #include "../DeferredShadingCommon.ush"
// #include "HairStrandsCommon.ush"

//#if SHADER_HAIRLUT 

// #include "../ShadingModels.ush"

#include "HairBsdf.csh"


#define PERMUTATION_LUT_TYPE_DUALSCATTERING 1
#define PERMUTATION_LUT_TYPE_MEAN_ENERGY 0
#define PERMUTATION_LUT_TYPE_NTT 2

#define PERMUTATION_LUT_TYPE PERMUTATION_LUT_TYPE_MEAN_ENERGY

cbuffer PrecomputeLUTData
{
	uint AbsorptionCount;
	uint RoughnessCount;
	uint ThetaCount;
	uint SampleCountScale;
};

RWTexture3D<float4>	OutputColor;

 float radicalInverse_VdC(uint bits) {
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
 }

float2 hammersley2d(uint i, uint N) 
{
	return float2(float(i)/float(N), radicalInverse_VdC(i));
}

// http://extremelearning.com.au/a-simple-method-to-construct-isotropic-quasirandom-blue-noise-point-sequences/
float2 R2Sequence( uint Index )
{
	const float Phi = 1.324717957244746;
	const float2 a = float2( 1.0 / Phi, 1.0 / Pow2(Phi) );
	return frac( a * Index );
}

// PDF = 1 / (4 * PI)
float4 UniformSampleSphere( float2 E )
{
	float Phi = 2 * PI * E.x;
	float CosTheta = 1 - 2 * E.y;
	float SinTheta = sqrt( 1 - CosTheta * CosTheta );

	float3 H;
	H.x = SinTheta * cos( Phi );
	H.y = SinTheta * sin( Phi );
	H.z = CosTheta;

	float PDF = 1.0 / (4 * PI);

	return float4( H, PDF );
}

float3 ToLinearAbsorption(float3 In) { return In*In; }

#define TILE_PIXEL_SIZE 8
#define JITTER_VIEW 0

#if PERMUTATION_LUT_TYPE == PERMUTATION_LUT_TYPE_DUALSCATTERING

// static int32 GHairLUTIncidentAngleCount = 64;
// static int32 GHairLUTRoughnessCount = 64;
// static int32 GHairLUTAbsorptionCount = 16;

[numthreads(TILE_PIXEL_SIZE, TILE_PIXEL_SIZE, TILE_PIXEL_SIZE)]
void CSMain(uint3 DispatchThreadId : SV_DispatchThreadID)
{

	// 3D LUT is organized as follow 
	//
	//      Z
	//	   ^
	//    /
	//   Absorption
	//  /
	// /
	//  ----- Theta ----> X
	// |
	// |
	// Roughness 
	// |
	// |
	// V
	// Y
	const uint3 PixelCoord = DispatchThreadId.xyz;

	const float SinAngle   	= saturate(float(PixelCoord.x+0.5f) / ThetaCount);
	const float Roughness  	= saturate(float(PixelCoord.y+0.5f) / RoughnessCount);
	const float Absorption 	= saturate(float(PixelCoord.z+0.5f) / AbsorptionCount);
	const float CosAngle 	= sqrt(1-SinAngle*SinAngle);

	FGBufferData GBufferData;
	GBufferData.Specular  	= 0.5f;
	GBufferData.BaseColor	= ToLinearAbsorption(Absorption.xxx);	// Perceptual absorption
	GBufferData.Metallic	= 0;		 							// This disable the fake multiple scattering
	GBufferData.Roughness 	= Roughness; 							// Perceptual roughness
	GBufferData.CustomData  = float4(0, 0, 1, 0); 					// Backlit

	float FrontHemisphereOutput = 0;
	float BackHemisphereOutput  = 0;

	uint FrontHemisphereCount = 0;
	uint BackHemisphereCount  = 0;
	
	const uint LocalThetaSampleCount	= max(1u, SampleCountScale * lerp(128, 64, Roughness));
	const uint LocalPhiSampleCount		= max(1u, SampleCountScale * lerp(128, 32, Roughness));
	const uint LocalViewSampleCount		= max(1u, SampleCountScale * 16);

	const float Area = 0;			// This is used for faking area light sources by increasing the roughness of the surface. Disabled = 0.
	const float Backlit = 1; 		// This is used for suppressing the R & TT terms when when the lighting direction comes from behind. Disabled = 1.
	const float3 N = float3(0,0,1); // N is the vector parallel to hair pointing toward root. I.e., the tangent T is up
	const float3 V = float3(CosAngle, 0, SinAngle);
	const float OpaqueVisibility = 1;
	FHairTransmittanceData TransmittanceData = InitHairStrandsTransmittanceData();
	TransmittanceData.bUseSeparableR = true;

	// const float MaxCosThetaRadius = cos(0.25f * PI / float(ThetaCount)); // [0, Pi/2] / ThetaCount which is divided by 2 for getting the actual radius
	// float3x3 ToViewBasis = GetTangentBasis(V);

	// #if JITTER_VIEW == 1
	// for (uint ViewIt=0; ViewIt<LocalViewSampleCount; ++ViewIt)
	// #endif
	for (uint SampleItY=0; SampleItY<LocalPhiSampleCount; ++SampleItY)
	for (uint SampleItX=0; SampleItX<LocalThetaSampleCount; ++SampleItX)
	{	
		// Sample a small solid around the view direction in order to average the small differences
		// This allows to fight undersampling for low roughnesses
		// #if JITTER_VIEW == 1
		// const float2 ViewU = Hammersley(ViewIt, LocalViewSampleCount, 0);
		// const float4 ViewSample = UniformSampleCone(ViewU, MaxCosThetaRadius);
		// const float3 JitteredV = mul(ViewSample, ToViewBasis);
		// const float ViewPdf = 1;
		// #else
		const float3 JitteredV = V;
		const float ViewPdf = 1;
		// #endif

		// Naive uniform sampling
		// @todo: important sampling of the Hair BSDF. The integration is too noisy for low roughness with uniform sampling
		const float2 jitter = R2Sequence(SampleItX + SampleItY * LocalThetaSampleCount); // float2(0.5f, 0.5f);
		const float2 u = (float2(SampleItX, SampleItY) + jitter) / float2(LocalThetaSampleCount, LocalPhiSampleCount);
		const float4 SampleDirection = UniformSampleSphere(u.yx);
		const float  SamplePdf = SampleDirection.w;
		const float3 L = SampleDirection.xyz;
        const float3 BSDFValue = HairShading(GBufferData, L, JitteredV, N, OpaqueVisibility, TransmittanceData, Backlit, Area, 0);
		
		// As in the original paper "Dual scattering approximation for fast multiple-scattering in hair", the average front/back scatter are cos-weighted (eq. 12). 
		const float CosL = 1.f;// abs(SampleDirection.x);

		// The view direction is aligned with the positive X Axis. This means:
		// * the back hemisphere (R / TRT) is on the positive side of X
		// * the front hemisphere (TT) is on the negative side of X
		const bool bIsBackHemisphere = SampleDirection.x > 0;
		if (bIsBackHemisphere)
		{
			BackHemisphereOutput += CosL * BSDFValue.x / SamplePdf;
			++BackHemisphereCount;
		}
		else
		{
			FrontHemisphereOutput += CosL * BSDFValue.x / SamplePdf;
			++FrontHemisphereCount;
		}
	}

	const float HemisphereFactor = 0.5f;
	OutputColor[PixelCoord] = float4(
		saturate(FrontHemisphereOutput / FrontHemisphereCount * HemisphereFactor), 
		saturate(BackHemisphereOutput / BackHemisphereCount * HemisphereFactor),
		0, 1);
}

#endif




#if PERMUTATION_LUT_TYPE == PERMUTATION_LUT_TYPE_MEAN_ENERGY

[numthreads(TILE_PIXEL_SIZE, TILE_PIXEL_SIZE, TILE_PIXEL_SIZE)]
void CSMain(uint3 DispatchThreadId : SV_DispatchThreadID)
{
	// 3D LUT is organized as follow 
	//
	//      Z
	//	   ^
	//    /
	//   Absorption
	//  /
	// /
	//  ----- Theta ----> X
	// |
	// |
	// Roughness 
	// |
	// |
	// V
	// Y
	const uint3 PixelCoord = DispatchThreadId.xyz;

	const float SinAngle = saturate(float(PixelCoord.x + 0.5f) / ThetaCount);
	const float Roughness = saturate(float(PixelCoord.y + 0.5f) / RoughnessCount);
	const float Absorption = saturate(float(PixelCoord.z + 0.5f) / AbsorptionCount);
	const float CosAngle = sqrt(1 - SinAngle * SinAngle);

	FGBufferData GBufferData;
	GBufferData.Specular = 0.5f;
	GBufferData.BaseColor = ToLinearAbsorption(Absorption.xxx);	// Perceptual absorption
	GBufferData.Metallic = 0;		 							// This disable the fake multiple scattering
	GBufferData.Roughness = Roughness; 							// Perceptual roughness
	GBufferData.CustomData = float4(0,0,1,0); 					// Backlit

	float R_Output = 0;
	float TT_Output = 0;
	float TRT_Output = 0;
	uint SampleCount = 0;

	const uint LocalThetaSampleCount = SampleCountScale * lerp(128, 64, Roughness);
	const uint LocalPhiSampleCount = SampleCountScale * lerp(128, 32, Roughness);
	const uint LocalViewSampleCount = SampleCountScale * 16;

	const float Area = 0;			// This is used for faking area light sources by increasing the roughness of the surface. Disabled = 0.
	const float Backlit = 1; 		// This is used for suppressing the R & TT terms when when the lighting direction comes from behind. Disabled = 1.
	const float3 N = float3(0, 0, 1); // N is the vector parallel to hair pointing toward root. I.e., the tangent T is up
	const float3 V = float3(CosAngle, 0, SinAngle);
	const float OpaqueVisibility = 1;
	FHairTransmittanceData Setting_R   = InitHairStrandsTransmittanceData(); Setting_R.ScatteringComponent		= HAIR_COMPONENT_R;
	FHairTransmittanceData Setting_TT  = InitHairStrandsTransmittanceData(); Setting_TT.ScatteringComponent		= HAIR_COMPONENT_TT;
	FHairTransmittanceData Setting_TRT = InitHairStrandsTransmittanceData(); Setting_TRT.ScatteringComponent	= HAIR_COMPONENT_TRT;

	// const float MaxCosThetaRadius = cos(0.25f * PI / float(ThetaCount)); // [0, Pi/2] / ThetaCount which is divided by 2 for getting the actual radius
	// float3x3 ToViewBasis = GetTangentBasis(V);

	// #if JITTER_VIEW == 1
	// for (uint ViewIt = 0; ViewIt < LocalViewSampleCount; ++ViewIt)
	// #endif
	for (uint SampleItY = 0; SampleItY < LocalPhiSampleCount; ++SampleItY)
	for (uint SampleItX = 0; SampleItX < LocalThetaSampleCount; ++SampleItX)
	{
		// Sample a small solid around the view direction in order to average the small differences
		// This allows to fight undersampling for low roughnesses
		// #if JITTER_VIEW == 1
		// const float2 ViewU = Hammersley(ViewIt, LocalViewSampleCount, 0);
		// const float4 ViewSample = UniformSampleCone(ViewU, MaxCosThetaRadius);
		// const float3 JitteredV = mul(ViewSample, ToViewBasis);
		// const float ViewPdf = 1;
		// #else
		const float3 JitteredV = V;
		const float ViewPdf = 1;
		// #endif

		// Naive uniform sampling
		// @todo: important sampling of the Hair BSDF. The integration is too noisy for low roughness with uniform sampling
		const float2 jitter = R2Sequence(SampleItX + SampleItY * LocalThetaSampleCount); // float2(0.5f, 0.5f);
		const float2 u = (float2(SampleItX, SampleItY) + jitter) / float2(LocalThetaSampleCount, LocalPhiSampleCount);
		const float4 SampleDirection = UniformSampleSphere(u.yx);
		const float  SamplePdf = SampleDirection.w;
		const float3 L = SampleDirection.xyz;
		const float3 BSDFValue_R   = HairShading(GBufferData, L, JitteredV, N, OpaqueVisibility, Setting_R, Backlit, Area, 0);
		const float3 BSDFValue_TT  = HairShading(GBufferData, L, JitteredV, N, OpaqueVisibility, Setting_TT, Backlit, Area, 0);
		const float3 BSDFValue_TRT = HairShading(GBufferData, L, JitteredV, N, OpaqueVisibility, Setting_TRT, Backlit, Area, 0);

		R_Output   += BSDFValue_R.x   / SamplePdf;
		TT_Output  += BSDFValue_TT.x  / SamplePdf;
		TRT_Output += BSDFValue_TRT.x / SamplePdf;
		++SampleCount;
	}

	OutputColor[PixelCoord] = float4(
		saturate(R_Output / SampleCount),
		saturate(TT_Output / SampleCount),
		saturate(TRT_Output / SampleCount),
		1);
}

#endif

#if PERMUTATION_LUT_TYPE == PERMUTATION_LUT_TYPE_NTT

// NTT LUT: 2D azimuthal TT pre-integration
//   X axis: U = 0.5 - cosThI * 0.5,  maps sinθ_i ∈ [0, 1]
//   Y axis: V = sqrt(betaTT),         maps roughness ∈ [0, sqrt(0.5)]
//   Output: .x = nttA (azimuthal TT coefficient), .y = nttB

RWTexture2D<float4> OutputNTT;

[numthreads(TILE_PIXEL_SIZE, TILE_PIXEL_SIZE, 1)]
void CSMain(uint3 DispatchThreadId : SV_DispatchThreadID)
{
    const uint2 PixelCoord = DispatchThreadId.xy;
    if (PixelCoord.x >= ThetaCount || PixelCoord.y >= RoughnessCount)
        return;

    // Reverse the UV mapping used in LineVertexShading.csh:
    //   nttUV = float2(0.5f - cosThI * 0.5f, sqrt(betaTT))
    const float U = saturate(float(PixelCoord.x + 0.5f) / ThetaCount);
    const float CosThI = 1.0f - 2.0f * U;
    const float SinThI = sqrt(saturate(1.0f - CosThI * CosThI));

    const float V_LUT = saturate(float(PixelCoord.y + 0.5f) / RoughnessCount);
    // V_LUT = sqrt(betaTT) = sqrt(roughness² * 0.5)  →  roughness = V_LUT * sqrt(2)
    const float Roughness = clamp(V_LUT * 1.41421356f, 0.001f, 1.0f);

    const float3 N = float3(0, 0, 1);
    // View direction: align theta_v with theta_i so half-angle θ_h ≈ θ_i
    const float3 V = float3(CosThI, 0, SinThI);

    // Unit absorption → Attenuation yields (1-F)² absorption-free TT
    FGBufferData GBufferData;
    GBufferData.BaseColor  = float3(1.0f, 1.0f, 1.0f);
    GBufferData.Specular   = 0.5f;
    GBufferData.Metallic   = 0;
    GBufferData.Roughness  = Roughness;
    GBufferData.CustomData = float4(0, 0, 1, 0);

    float nttA = 0;
    float nttB = 0;
    uint  SampleCount = 0;

    const uint LocalThetaSampleCount = max(1u, SampleCountScale * lerp(128, 64, Roughness));
    const uint LocalPhiSampleCount   = max(1u, SampleCountScale * lerp(128, 32, Roughness));

    for (uint SampleItY = 0; SampleItY < LocalPhiSampleCount; ++SampleItY)
    for (uint SampleItX = 0; SampleItX < LocalThetaSampleCount; ++SampleItX)
    {
        const float2 jitter = R2Sequence(SampleItX + SampleItY * LocalThetaSampleCount);
        const float2 u = (float2(SampleItX, SampleItY) + jitter) / float2(LocalThetaSampleCount, LocalPhiSampleCount);
        const float4 SampleDir = UniformSampleSphere(u.yx);
        const float  SamplePdf = SampleDir.w;
        const float3 L = SampleDir.xyz;

        // TT-only BSDF via reference path (HAIR_COMPONENT_TT)
        float3 BSDF_TT = HairShadingRef(GBufferData, L, V, N, uint2(0, 0), HAIR_COMPONENT_TT);

        // Divide by runtime longitudinal Gaussian to extract azimuthal coefficient.
        // Parameters match LineVertexShading.csh:
        //   betaTT_w = Roughness² * 0.5
        //   M_TT = LongitudinalGaussian(thetaH - shift*0.5, betaTT_w² * 0.5)
        float SinThL = dot(N, L);
        float SinThV = dot(N, V);
        float ThetaI = asin(clamp(SinThL, -1.0f, 1.0f));
        float ThetaR = asin(clamp(SinThV, -1.0f, 1.0f));
        float ThetaH = (ThetaI + ThetaR) * 0.5f;
        const float Shift    = 0.035f;
        float BetaTT_w       = Roughness * Roughness * 0.5f;
        float VarTT          = BetaTT_w * BetaTT_w * 0.5f;
        float M_TT           = LongitudinalGaussian(ThetaH - Shift * 0.5f, VarTT);
        M_TT                 = max(M_TT, 1e-10f);

        nttA += BSDF_TT.x / (M_TT * SamplePdf);
        nttB += BSDF_TT.y / (M_TT * SamplePdf);
        ++SampleCount;
    }

    const float Norm = 1.0f / max(float(SampleCount), 1.0f);
    OutputNTT[PixelCoord] = float4(nttA * Norm, nttB * Norm, 0, 1);
}

#endif

// #endif // SHADER_HAIRLUT
