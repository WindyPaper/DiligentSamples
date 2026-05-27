// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct FGBufferData
{
	// 0..1, white for SHADINGMODELID_SUBSURFACE_PROFILE and SHADINGMODELID_EYE (apply BaseColor after scattering is more correct and less blurry)
	float3 BaseColor;
	// 0..1
	float Metallic;
	// 0..1
	float Specular;
	// 0..1
	float4 CustomData;
	// 0..1
	float Roughness;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Hair components

// Hair reflectance component (R, TT, TRT, Local Scattering, Global Scattering, Multi Scattering,...)
#define HAIR_COMPONENT_R			0x1u
#define HAIR_COMPONENT_TT			0x2u
#define HAIR_COMPONENT_TRT			0x4u
#define HAIR_COMPONENT_LS			0x8u 
#define HAIR_COMPONENT_GS			0x10u
#define HAIR_COMPONENT_MULTISCATTER	0x20u
#define HAIR_COMPONENT_TT_MODEL  	0x40u

#define PI 3.141592653f

///////////////////////////////////////////////////////////////////////////////////////////////////
// Transmittance functions

struct FHairTransmittanceData
{
	bool bUseLegacyAbsorption;
	bool bUseSeparableR;
	bool bUseBacklit;

	float  OpaqueVisibility;
	float3 LocalScattering;
	float3 GlobalScattering;

	uint ScatteringComponent;
};

//==================
float Square( float x )
{
	return x*x;
}

float Pow2( float x )
{
	return x*x;
}

float3 Pow2( float3 x )
{
	return x*x;
}

float Pow3( float x )
{
	return x*x*x;
}

float2 Pow3( float2 x )
{
	return x*x*x;
}

float3 Pow3( float3 x )
{
	return x*x*x;
}

float Pow5( float x )
{
	float xx = x*x;
	return xx * xx * x;
}

float Luminance( float3 LinearColor )
{
	return dot( LinearColor, float3( 0.3, 0.59, 0.11 ) );
}

//==================

FHairTransmittanceData InitHairTransmittanceData(bool bMultipleScatterEnable = true)
{
	FHairTransmittanceData o;
	o.bUseLegacyAbsorption = true;
	o.bUseSeparableR = true;
	o.bUseBacklit = false;

	o.OpaqueVisibility = 1;
	o.LocalScattering = 0;
	o.GlobalScattering = 1;
	o.ScatteringComponent = HAIR_COMPONENT_R | HAIR_COMPONENT_TT | HAIR_COMPONENT_TRT | (bMultipleScatterEnable ? HAIR_COMPONENT_MULTISCATTER : 0);

	return o;
}

FHairTransmittanceData InitHairStrandsTransmittanceData(bool bMultipleScatterEnable = false)
{
	FHairTransmittanceData o = InitHairTransmittanceData(bMultipleScatterEnable);
	o.bUseLegacyAbsorption = false;
	o.bUseBacklit = true;
	return o;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Hair Absorption
 
// Reference: A Practical and Controllable Hair and Fur Model for Production Path Tracing.
float3 HairAbsorptionToColor(float3 A, float B=0.3f)
{
	const float b2 = B * B;
	const float b3 = B * b2;
	const float b4 = b2 * b2;
	const float b5 = B * b4;
	const float D = (5.969f - 0.215f * B + 2.532f * b2 - 10.73f * b3 + 5.574f * b4 + 0.245f * b5);
	return exp(-sqrt(A) * D);
}

// Reference: A Practical and Controllable Hair and Fur Model for Production Path Tracing.
float3 HairColorToAbsorption(float3 C, float B = 0.3f)
{
	const float b2 = B * B;
	const float b3 = B * b2;
	const float b4 = b2 * b2;
	const float b5 = B * b4;
	const float D = (5.969f - 0.215f * B + 2.532f * b2 - 10.73f * b3 + 5.574f * b4 + 0.245f * b5);
	return Pow2(log(C) / D);
}

// Reference: An Energy-Conserving Hair Reflectance Model
// Adapated for [0..1] range
float3 GetHairColorFromMelanin(float InMelanin, float InRedness, float3 InDyeColor)
{
	InMelanin = saturate(InMelanin);
	InRedness = saturate(InRedness);
	const float Melanin		= -log(max(1 - InMelanin, 0.0001f));
	const float Eumelanin 	= Melanin * (1 - InRedness);
	const float Pheomelanin = Melanin * InRedness;

	const float3 DyeAbsorption = HairColorToAbsorption(saturate(InDyeColor));
	const float3 Absorption = Eumelanin * float3(0.506f, 0.841f, 1.653f) + Pheomelanin * float3(0.343f, 0.733f, 1.924f);

	return HairAbsorptionToColor(Absorption + DyeAbsorption);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Compute-path utilities  (used by HairShadeCS.csh)

// ---- sRGB -> linear (IEC 61966-2-1) ----
float SrgbToLinear(float c)
{
    return (c <= 0.04045f)
        ? (c * 0.0773994f)
        : exp2(log2((c + 0.055f) * 0.947867f) * 2.4f);
}

float3 SrgbToLinear3(float3 c)
{
    return float3(SrgbToLinear(c.x), SrgbToLinear(c.y), SrgbToLinear(c.z));
}

// ---- HSV -> RGB (branch-free, frac/saturate) ----
float3 HsvToRgb(float h, float s, float v)
{
    float3 rgb;
    rgb.x = saturate(abs(frac(h)          * 6.0f - 3.0f) - 1.0f);
    rgb.y = saturate(abs(frac(h + 0.6667f) * 6.0f - 3.0f) - 1.0f);
    rgb.z = saturate(abs(frac(h + 0.3333f) * 6.0f - 3.0f) - 1.0f);
    return ((rgb - 1.0f) * s + 1.0f) * v;
}

// ---- Pack R11G11B10 (half-float bit-shift) ----
uint PackR11G11B10(float r, float g, float b)
{
    uint pr = (f32tof16(r) >> 4) & 0x7FFu;
    uint pg = (f32tof16(g) >> 4) & 0x7FFu;
    uint pb = (f32tof16(b) >> 5) & 0x3FFu;
    return pr | (pg << 11u) | (pb << 22u);
}

// ---- NaN-safe helpers ----
float SafeMax(float a, float b)
{
    return (isnan(a)) ? b : (isnan(b) ? a : max(a, b));
}

float SafeMin(float a, float b)
{
    return (isnan(a)) ? b : (isnan(b) ? a : min(a, b));
}

// ---- Marschner longitudinal Gaussian (exp2/log2 form) ----
// N(x; 0, variance) unnormalized term:  exp(-x^2 / (2*variance)) / sqrt(2*pi*variance)
float LongitudinalGaussian(float h_minus_mean, float variance)
{
    return exp2((-(h_minus_mean * h_minus_mean) / (variance * 2.0f)) * 1.44269502f)
         / sqrt(variance * 6.28318548f);
}
