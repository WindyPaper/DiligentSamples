//RayMarch.fx
//Performs ray marching and computes shadowed scattering

#include "BasicStructures.fxh"
#include "EpipolarLightScattering/AtmosphereShadersCommon.fxh"

cbuffer cbParticipatingMediaScatteringParams
{
    AirScatteringAttribs g_MediaParams;
}

cbuffer cbCameraAttribs
{
    CameraAttribs g_CameraAttribs;
}

cbuffer cbLightParams
{
    LightAttribs g_LightAttribs;
}

cbuffer cbPostProcessingAttribs
{
    EpipolarLightScatteringAttribs g_PPAttribs;
};

cbuffer cbMiscDynamicParams
{
    MiscDynamicParams g_MiscParams;
}

Texture2D<float2> g_tex2DOccludedNetDensityToAtmTop;
SamplerState      g_tex2DOccludedNetDensityToAtmTop_sampler;

Texture2D<float>  g_tex2DEpipolarCamSpaceZ;

#if ENABLE_LIGHT_SHAFTS

#   if USE_1D_MIN_MAX_TREE
    Texture2D<float4> g_tex2DSliceUVDirAndOrigin;
    Texture2D<float2> g_tex2DMinMaxLightSpaceDepth;
#   endif

// Texture2DArray<float>  g_tex2DLightSpaceDepthMap;
// SamplerComparisonState g_tex2DLightSpaceDepthMap_sampler;

#endif

Texture2D<float2> g_tex2DCoordinates;

Texture2D<float>  g_tex2DCamSpaceZ;
SamplerState      g_tex2DCamSpaceZ_sampler;

Texture2D<float4> g_tex2DColorBuffer;

Texture2D<float>  g_tex2DAverageLuminance;

Texture3D<float3> g_tex3DSingleSctrLUT;
SamplerState      g_tex3DSingleSctrLUT_sampler;

Texture3D<float3> g_tex3DHighOrderSctrLUT;
SamplerState      g_tex3DHighOrderSctrLUT_sampler;

Texture3D<float3> g_tex3DMultipleSctrLUT;
SamplerState      g_tex3DMultipleSctrLUT_sampler;


#include "EpipolarLightScattering/LookUpTables.fxh"
#include "EpipolarLightScattering/ScatteringIntegrals.fxh"
#include "EpipolarLightScattering/Extinction.fxh"
#include "EpipolarLightScattering/UnshadowedScattering.fxh"
//#include "EpipolarLightScattering/ToneMapping.fxh"


void RayMarchPS(in FullScreenTriangleVSOutput VSOut,
                out float4 f4Inscattering : SV_TARGET)
{
    uint2 ui2SamplePosSliceInd = uint2(VSOut.f4PixelPos.xy);
    float2 f2SampleLocation = g_tex2DCoordinates.Load( int3(ui2SamplePosSliceInd, 0) );
    float fRayEndCamSpaceZ = g_tex2DEpipolarCamSpaceZ.Load( int3(ui2SamplePosSliceInd, 0) );

    [branch]
    if( any( Greater( abs( f2SampleLocation ), (1.0 + 1e-3) * float2(1.0, 1.0)) ) )
    {
        f4Inscattering = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }
    f4Inscattering = float4(1.0, 1.0, 1.0, 1.0);

    float3 f3Extinction;
    ComputeUnshadowedInscattering(f2SampleLocation,
                                  fRayEndCamSpaceZ,
                                  g_PPAttribs.uiInstrIntegralSteps,
                                  g_PPAttribs.f4EarthCenter.xyz,
                                  f4Inscattering.rgb,
                                  f3Extinction);
    f4Inscattering.rgb *= g_LightAttribs.f4Intensity.rgb;

}


//float3 FixInscatteredRadiancePS(FullScreenTriangleVSOutput VSOut) : SV_Target
//{
//    if( g_PPAttribs.bShowDepthBreaks )
//        return float3(0,1,0);
//
//    float fCascade = g_MiscParams.fCascadeInd + VSOut.fInstID;
//    float fRayEndCamSpaceZ = g_tex2DCamSpaceZ.SampleLevel( samLinearClamp, ProjToUV(VSOut.f2NormalizedXY.xy), 0 );
//
//#if ENABLE_LIGHT_SHAFTS
//    return ComputeShadowedInscattering(VSOut.f2NormalizedXY.xy, 
//                              fRayEndCamSpaceZ,
//                              fCascade,
//                              false, // We cannot use min/max optimization at depth breaks
//                              0 // Ignored
//                              );
//#else
//    float3 f3Inscattering, f3Extinction;
//    ComputeUnshadowedInscattering(VSOut.f2NormalizedXY.xy, fRayEndCamSpaceZ, g_PPAttribs.uiInstrIntegralSteps, g_PPAttribs.f4EarthCenter.xyz, f3Inscattering, f3Extinction);
//    f3Inscattering *= g_LightAttribs.f4Intensity.rgb;
//    return f3Inscattering;
//#endif
//
//}


void FixAndApplyInscatteredRadiancePS(FullScreenTriangleVSOutput VSOut,
                                      out float4 f4Color : SV_Target)
{
    f4Color = float4(0.0, 1.0, 0.0, 1.0);
    if( g_PPAttribs.bShowDepthBreaks )
        return;

    float fCamSpaceZ = g_tex2DCamSpaceZ.SampleLevel(g_tex2DCamSpaceZ_sampler, NormalizedDeviceXYToTexUV(VSOut.f2NormalizedXY), 0 );
    float3 f3BackgroundColor = float3(0.0, 0.0, 0.0);
    [branch]
    if( !g_PPAttribs.bShowLightingOnly )
    {
        f3BackgroundColor = g_tex2DColorBuffer.Load( int3(VSOut.f4PixelPos.xy,0) ).rgb;
        f3BackgroundColor *= (fCamSpaceZ > g_CameraAttribs.fFarPlaneZ) ? g_LightAttribs.f4Intensity.rgb : float3(1.0, 1.0, 1.0);
        float3 f3ReconstructedPosWS = ProjSpaceXYZToWorldSpace(float3(VSOut.f2NormalizedXY.xy, fCamSpaceZ), g_CameraAttribs.mProj, g_CameraAttribs.mViewProjInv);
        float3 f3Extinction = GetExtinction(g_CameraAttribs.f4Position.xyz, f3ReconstructedPosWS, g_PPAttribs.f4EarthCenter.xyz,
                                            g_MediaParams.fAtmBottomRadius, g_MediaParams.fAtmTopRadius, g_MediaParams.f4ParticleScaleHeight);
        f3BackgroundColor *= f3Extinction.rgb;
    }
    
    float fCascade = g_MiscParams.fCascadeInd + VSOut.fInstID;


    float3 f3InsctrColor, f3Extinction;
    ComputeUnshadowedInscattering(VSOut.f2NormalizedXY.xy,
                                  fCamSpaceZ,
                                  g_PPAttribs.uiInstrIntegralSteps,
                                  g_PPAttribs.f4EarthCenter.xyz,
                                  f3InsctrColor,
                                  f3Extinction);
    f3InsctrColor *= g_LightAttribs.f4Intensity.rgb;

    f4Color.rgb = (f3BackgroundColor + f3InsctrColor);
// #if PERFORM_TONE_MAPPING
//     float fAveLogLum = GetAverageSceneLuminance(g_tex2DAverageLuminance);
//     f4Color.rgb = ToneMap(f3BackgroundColor + f3InsctrColor, g_PPAttribs.ToneMapping, fAveLogLum);
// #else
    const float MinLumn = 0.01;
    float2 LogLum_W = GetWeightedLogLum(f3BackgroundColor + f3InsctrColor, MinLumn);
    f4Color.rgb = float3(LogLum_W.x, LogLum_W.y, 0.0);
//#endif
}
