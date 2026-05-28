// hair_calcShading.hlsl
// Reconstructed from: hair_shade_result1.bin (DXIL, CS_6_2)
// Entry point: calcShading()   [numthreads(64,1,1)]
// Source hash:  05e9fa138167205c826a48bee8d36bcc
//
// Pipeline: Compute Shader — hair strand shading
//   - Reads Visibility/CompactLineIndices/Vertices buffers
//   - Evaluates directional + punctual lights with shadow, IBL, Deep Shadow multi-scatter
//   - Writes packed uint result to RWShade[threadId]
//
// ─────────────────────────────────────────────────────────────────────────────
// Constant Buffers
// ─────────────────────────────────────────────────────────────────────────────

cbuffer SceneInfo : register(b0)
{
    row_major float4x4 viewProjMat;            // Offset:   0
    row_major float3x4 transposeViewMat;       // Offset:  64
    row_major float3x4 transposeViewInvMat;    // Offset: 112
    float4 projElement[2];                     // Offset: 160
    float4 projInvElements[2];                 // Offset: 192
    row_major float4x4 viewProjInvMat;         // Offset: 224
    row_major float4x4 prevViewProjMat;        // Offset: 288
    float3 ZToLinear;                          // Offset: 352
    float subdivisionLevel;                    // Offset: 364
    float2 screenSize;                         // Offset: 368
    float2 screenInverseSize;                  // Offset: 376
    float2 cullingHelper;                      // Offset: 384
    float cameraNearPlane;                     // Offset: 392
    float cameraFarPlane;                      // Offset: 396
    float4 viewFrustum[6];                     // Offset: 400
    float4 clipplane;                          // Offset: 496
    float2 vrsVelocityThreshold;               // Offset: 512
    uint GPUVisibleMask;                       // Offset: 520
    uint resolutionRatioPacked;                // Offset: 524
};

cbuffer RootConstant : register(b0, space32)
{
    uint constant32Bits;   // total strand count / dispatch bound
};

cbuffer LightInfo : register(b1)
{
    uint PunctualLightCount;
    uint AreaLightCount;
    uint PunctualLightFowardCount;
    uint AreaLightFowardCount;
    float2 LightCullingScreenSize;
    float2 InverseLightCullingScreenSize;
    float LightCullingOffsetScale;
    uint RT_PunctualLightCount;
    uint RT_AreaLightCount;
    uint CubemapArrayCount;
    float2 ShadowMapRes;
    float2 InverseShadowMapRes;
    float3 DL_Direction;
    uint DL_Enable;
    float3 DL_Color;
    float DL_SpecularControl;
    float3 DL_VolumetricScatteringColor;
    float DL_PCSS_KERNEL;
    row_major float4x4 DL_TextureProjection;
    uint DL_TextureBindlessIndex;
    float DL_ReceiverSlopeBiasScale;
    uint DL_Reserved;
    float DL_ContactShadow;
    row_major float4x4 DL_ViewProjection;
    float DL_Variance;
    uint DL_ArrayIndex;
    uint DL_TranslucentArrayIndex;
    float DL_Bias;
    float4 DL_ZToLinear;
    float3 Cascade_Translate1;
    float Cascade_Bias1;
    float3 Cascade_Translate2;
    float Cascade_Bias2;
    float3 Cascade_Translate3;
    float Cascade_Bias3;
    float2 Cascade_Scale1;
    float2 Cascade_Scale2;
    float2 Cascade_Scale3;
    float Cascade_FadeBorder;
    uint SDSMEnable;
    float4 CascadeDistance;
    float3 Atmopshere_Reserved;
    uint Atmopshere_Flags;
    float3 SDFShadowTranslate;
    float SDFShadowNearFarRatio;
    uint SDFShadowEnabled;
    float SDFShadowReserve1;
    float SDFShadowReserve2;
    float SDFShadowReserve3;
    uint lightProbeOffset;
    uint sparseLightProbeAreaNum;
    uint tetNumMinus1;
    uint sparseTetNumMinus1;
    float smoothStepRateMinus;
    float smoothStepRateRcp;
    float worldPositionOffsetBias;
    float _LightProbeReserve1;
    float3 AOTint;
    uint CapsuleLightCount;
    float3 LightProbe_WorldOffset;
    float ReflectionProbeBoost;
};

cbuffer ShadowSamplingRotation : register(b2)
{
    float4 ShadowSamplePoints[8];
};

cbuffer Material : register(b3)
{
    float3 hm_sigma;                    // absorption coefficients RGB
    float  hm_IOR;
    float  hm_cuticleTiltAngle;         // cuticle tilt (radians)
    float  hm_s;
    float  hm_v;
    float  hm_m0_roughness;             // M-lobe R roughness
    float  hm_f0;                       // Fresnel F0
    float  hm_sqrt_s;
    float  hm_sqrt_v;
    float  hm_sqrt_roughness;           // sqrt of roughness β
    float  hm_fakeMultipleScatteringFactor;
    float  hm_sigmaRndX;
    float  hm_sigmaRndY;
    float  hm_sigmaRndZ;
    float3 hm_sigmaTip;
    float  hm_tipRoughness;
    float3 hm_sigmaTipRnd;
    float  hm_rndRoughness;
    float  hm_scaleWidth;
    float  hm_minWidth;
    float  hm_maxWidth;
    float  hm_backscatterScale;
    float  hm_global_transparency;
    float  hm_shadowDensity;
    float  hm_shadowPow;
    uint   hm_guideShadingQualityLightMaxNum;
    float  hm_guideShadingAdjustShading;
    float  hm_guideShadingAdjustDensity;
    float  hm_guideShadingQualityLightMaxNumOverDensity;
    float  hm_depthwriteWithAlphaBlendingThrehold;
    uint   hm_projLightNum;
};

cbuffer DSInfo : register(b4)
{
    float DSInfo_VoxelWorldSize;
    uint  DSInfo_VolumeResolution;
    uint  DSInfo_VolumePageResolution;
    float DSInfo_RasterDepthThreshold;
    uint  DSInfo_LUTThetaCount;
    uint  DSInfo_LUTRoughnessCount;
    uint  DSInfo_LUTAbsorptionCount;
    float DSInfo_VolumeTracingOffsetScale;
    float DSInfo_VolumeTracingDelta;
    float DSInfo_RasterStrandWidthScale;
    float DSInfo_StrandWidthMin;
    float DSInfo_StrandWidthMax;
    float DSInfo_StrandWidthAve;
    float DSInfo_VolumeTracingIBLDelta;
};

cbuffer IBLDMTInfo : register(b5)
{
    uint  IBLDMTInfo_IBLDirectionStart;
    uint  IBLDMTInfo_IBLDirectionNum;
    float IBLDMTInfo_IBLRouhness;
    uint  IBLDMTInfo_Padding;
    float3 IBLDMTInfo_CenterPos;
    float IBLDMTInfo_InterpRate;
    float IBLDMTInfo_IndirectLightingMultiplier;
    float IBLDMTInfo_AlbedoBlendRate;
    uint2 dummyData;
};

cbuffer UserMaterial : register(b6)
{
    float4 VAR_ColorA;
    float4 VAR_ColorB;
    float4 VAR_ColorC;
    float4 VAR_ColorPoint;
    float  VAR_Hue_Random;
    float  VAR_Hue_Offset;
    float  VAR_Saturate_Random;
    float  VAR_Saturate_Offset;
    float  VAR_Value_Random;
    float  VAR_Value_Offset;
    float  CAPCOM_MATERIAL_RESERVE0;
    float  CAPCOM_MATERIAL_RESERVE1;
};

// ─────────────────────────────────────────────────────────────────────────────
// Samplers
// ─────────────────────────────────────────────────────────────────────────────

SamplerState            BilinearClamp   : register(s5,  space32);
SamplerState            BilinearMirror  : register(s7,  space32);
SamplerState            TrilinearMirror : register(s11, space32);
SamplerComparisonState  LinearCompare   : register(s12, space32);

// ─────────────────────────────────────────────────────────────────────────────
// Texture / Buffer Resources
// ─────────────────────────────────────────────────────────────────────────────

Texture2DArray<float>               BlueNoise32          : register(t0);   // Blue noise, 2darray f32
StructuredBuffer<IBLCubemapArrayInfo2> IBLCubemapArrayList2SRV : register(t1);
StructuredBuffer<LightParameter>    LightParameterSRV    : register(t2);
StructuredBuffer<ShadowParameter>   ShadowParameterSRV   : register(t3);
Texture3D<uint>                     LightCullingVolumeSRV: register(t4);   // 3d u32 light culling
ByteAddressBuffer                   LightCullingListSRV  : register(t5);
Texture2D<float3>                   AmbientBRDF          : register(t6);   // 2d f32 (float3: R,G,B channels used)
ByteAddressBuffer                   BSPTree              : register(t7);
StructuredBuffer<TetrahedronTransform> TetraCoordinate   : register(t8);
ByteAddressBuffer                   DepthBlocker         : register(t9);
ByteAddressBuffer                   IndirectProbe        : register(t10);
StructuredBuffer<EnvironmentBVH>    IBLCubemapBVHSRV     : register(t11);
TextureCube<float4>                 CubemapSRV           : register(t12);
Texture2DArray<float>               StaticShadowMapSRV   : register(t13);  // 2darray f32
Texture2DArray<float>               ShadowMapSRV         : register(t14);  // 2darray f32
Texture1DArray<float>               IESLightTableSRV     : register(t15);
StructuredBuffer<uint>              CompactLineIndices   : register(t16);
StructuredBuffer<LineVertices>      Vertices             : register(t17);
StructuredBuffer<uint>              Visibility           : register(t18);
Texture2D<float2>                   Lut_Ntt              : register(t19);  // R16G16_FLOAT: .x=N_r, .y=N_tt
StructuredBuffer<DSVolumeInfo>      DSVolumeInfoBuffer   : register(t20);
Texture3D<uint>                     DSVolumeTexture      : register(t21);  // 3d u32 deep shadow density
Texture3D<float4>                   DSLUT                : register(t22);  // 3d f32x4: (N_R,N_TT,N_TRT_a,N_TRT_b)

RWStructuredBuffer<uint>            RWShade              : register(u0);   // output packed uint

// ─────────────────────────────────────────────────────────────────────────────
// Struct Definitions
// ─────────────────────────────────────────────────────────────────────────────

struct IBLCubemapArrayInfo2
{
    row_major float3x4 obbTest;    // OBB test matrix (3×4)
    float4 centerIndex;            // xyz=center, w=array index
};

struct LightParameter
{
    float3 position;
    float  boundingRadius;
    float3 direction;
    float  falloff;
    float4 attenuation;
    float3 color;
    float  tolerance;
    uint   shadowIndex;
    uint   iesId;
    uint2  reserved;
    float4 ext;
};

struct ShadowParameter
{
    row_major float4x4 viewProjection;
    float  variance;
    uint   arrayIndex;
    uint   translucentArrayIndex;
    float  bias;
    float2 renderClipPlane;
    float2 reserved;
    float4 zToLinear;
};

struct TetrahedronTransform
{
    uint4  vertexId;
    uint4  neighborTetId;
    float4 row0;
    float4 row1;
    float4 row2;
};

struct EnvironmentBVH
{
    float4 data1;
    float4 data2;
};

struct LineVertices
{
    float3 p;
    uint   misc;   // bits[31:28]=flags, bits[27:0]=index
};

struct DSVolumeInfo
{
    float3 mMinAABB;
    float3 mMaxAABB;
    uint3  mResolution;
    uint3  mClearResolution;
    float3 mScale;
    float3 mInvLength;
    float3 mInvResolution;
};

// ─────────────────────────────────────────────────────────────────────────────
// Helper: Fresnel Schlick
// ─────────────────────────────────────────────────────────────────────────────

float FresnelSchlick(float cosTheta, float f0)
{
    float x = 1.0f - cosTheta;
    float x2 = x * x;
    float x5 = x2 * x2 * x;
    return f0 + (1.0f - f0) * x5;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: Marschner Longitudinal M-lobe  (unnormalized Gaussian)
// ─────────────────────────────────────────────────────────────────────────────

float HairM(float sinThetaI, float sinThetaR, float beta)
{
    // M(θi,θr) = Gauss(sinθi - sinθr, 2β²)  (simplified)
    float diff = sinThetaI - sinThetaR;
    float v = 2.0f * beta * beta;
    return exp(-diff * diff / max(v, 1e-5f)) / sqrt(6.28318530718f * v);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: sRGB color encoding
//   DXIL pattern: log(σ+0.05307) * 0.947206, then exp(result * 2.4)
//   Approximates sRGB gamma encode; used for hair color sigma->albedo conversion
// ─────────────────────────────────────────────────────────────────────────────

float SigmaToAlbedoChannel(float sigma)
{
    // DXIL constants: 0x3FAC28F5C = 0.05307..., 0x3FEE54EDE = 0.94720..., 0x4003333... = 2.4
    float logVal = log((sigma + 0.05307f) * 0.94720f) * 2.4f;
    float powered = exp(logVal);
    // Branch: sigma > 0.04f → use powered, else linear approx
    return (sigma > 0.04f) ? powered : sigma * 0.15441f;
}

float3 SigmaToAlbedo(float3 sigma)
{
    return float3(
        SigmaToAlbedoChannel(sigma.x),
        SigmaToAlbedoChannel(sigma.y),
        SigmaToAlbedoChannel(sigma.z)
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: Deep Shadow transmittance via DSVolumeTexture
//   Samples DSVolumeTexture at world position → Beer-Lambert absorption
// ─────────────────────────────────────────────────────────────────────────────

float3 DeepShadowTransmittance(float3 worldPos, float3 absorb_TT,
                               DSVolumeInfo dsInfo, uint mipLevel)
{
    // voxel coordinate
    float3 uvw = (worldPos - dsInfo.mMinAABB) * dsInfo.mInvLength;
    int3   coord = (int3)(uvw * (float3)dsInfo.mResolution);

    uint packed = DSVolumeTexture.Load(int4(coord, mipLevel));
    uint density24 = packed & 0x00FFFFFFu;

    // Beer-Lambert: transmittance = exp(-2σ * density)
    float density = (float)density24 / 16777215.0f;   // normalize 24-bit → [0,1]
    float3 transmittance = exp(-2.0f * absorb_TT * density * 100.0f);
    return transmittance;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: Hair Multi-Scatter via DSLUT
//   DSLUT = Texture3D<float4>(N_R, N_TT, N_TRT_a, N_TRT_b)
//   Axes:  x=sinθ (0..1), y=β roughness (0..1), z=σ absorption (0..1)
// ─────────────────────────────────────────────────────────────────────────────

float3 HairMultiScatterDSLUT(float sinThetaI, float beta, float3 sigma,
                              float M_R, float M_TT, float M_TRT)
{
    // map absorption to LUT z-coordinate
    float betaSat = saturate(beta);
    float sinSat  = saturate(abs(sinThetaI));

    // three DSLUT samples at different absorption offsets (IBL integration strata)
    float delta0 = DSInfo_VolumeTracingDelta;
    float delta1 = DSInfo_VolumeTracingIBLDelta;

    float3 uvw0 = float3(sinSat, betaSat, saturate(sigma.x));
    float3 uvw1 = float3(sinSat, betaSat, saturate(sigma.y));
    float3 uvw2 = float3(sinSat, betaSat, saturate(sigma.z));

    float4 lut0 = DSLUT.SampleLevel(BilinearClamp, uvw0, 0);
    float4 lut1 = DSLUT.SampleLevel(BilinearClamp, uvw1, 0);
    float4 lut2 = DSLUT.SampleLevel(BilinearClamp, uvw2, 0);

    // N_R, N_TT, N_TRT from LUT (per channel)
    float3 N_R   = float3(lut0.x, lut1.x, lut2.x);
    float3 N_TT  = float3(lut0.y, lut1.y, lut2.y);
    float3 N_TRT = float3(lut0.z + lut0.w, lut1.z + lut1.w, lut2.z + lut2.w);

    // Energy-conserved multi-scatter combination
    float3 f_single = M_R * N_R + M_TT * N_TT + M_TRT * N_TRT;

    // Fake multiple scattering compensation
    float3 E_ms = f_single * hm_fakeMultipleScatteringFactor;

    return f_single + E_ms;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: Shadow map lookup (cascade directional)
//   Performs 5-tap PCF over ShadowSamplePoints in ShadowMapSRV (Texture2DArray)
// ─────────────────────────────────────────────────────────────────────────────

float SampleDirectionalShadow(float3 shadowUV, float shadowDepth, uint arrayIdx,
                               float bias)
{
    float shadowSum  = 0.0f;
    float sampleCount = 0.0f;
    float invShadowRes = InverseShadowMapRes.x;

    for (int i = 0; i < 5; i++)
    {
        float2 offset = ShadowSamplePoints[i].xy * invShadowRes;
        float2 uv     = shadowUV.xy + offset;
        float  uvMax  = max(abs(uv.x - 0.5f), abs(uv.y - 0.5f));
        bool   inBound = (uvMax <= 0.5f);

        if (inBound)
        {
            float cmp = ShadowMapSRV.SampleCmpLevelZero(
                LinearCompare, float3(uv, (float)arrayIdx), shadowDepth - bias);
            shadowSum  += 1.0f - cmp;
            sampleCount += 1.0f;
        }
    }
    return (sampleCount > 0.0f) ? (shadowSum / sampleCount) : 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: Punctual light shadow (ShadowParameterSRV + ShadowMapSRV)
// ─────────────────────────────────────────────────────────────────────────────

float SamplePunctualShadow(float3 worldPos, uint shadowIdx)
{
    ShadowParameter sp = ShadowParameterSRV[shadowIdx];
    float4 shadowClip = mul(float4(worldPos, 1.0f), sp.viewProjection);
    float3 shadowNdc  = shadowClip.xyz / shadowClip.w;
    float2 shadowUV   = shadowNdc.xy * float2(0.5f, -0.5f) + 0.5f;
    float  shadowZ    = shadowNdc.z;

    float cmp = ShadowMapSRV.SampleCmpLevelZero(
        LinearCompare,
        float3(shadowUV, (float)sp.arrayIndex),
        shadowZ - sp.bias);
    return 1.0f - cmp;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: IES light falloff table lookup
// ─────────────────────────────────────────────────────────────────────────────

float SampleIESTable(uint iesId, float angle)
{
    return IESLightTableSRV.SampleLevel(BilinearClamp, float2(angle, (float)iesId), 0).x;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: Cascade shadow selection and blend
//   Returns (shadowVisibility, arrayIndex) for directional light
// ─────────────────────────────────────────────────────────────────────────────

float DirectionalShadow(float3 worldPos, float linearDepth)
{
    // Choose cascade from CascadeDistance
    // Cascade 1
    float3 sc1 = worldPos * float3(Cascade_Scale1, 0.0f) + float3(Cascade_Translate1.xy, 0.0f);
    // ... (cascade logic abbreviated — full DXIL has standard RE Engine 3-cascade PCF)
    // Reconstruct shadow UV for selected cascade
    float4 shadowClip = mul(float4(worldPos, 1.0f), DL_ViewProjection);
    float3 shadowNdc  = shadowClip.xyz / shadowClip.w;
    float2 shadowUV   = shadowNdc.xy * float2(0.5f, -0.5f) + 0.5f;
    float  shadowZ    = shadowNdc.z;

    float vis = SampleDirectionalShadow(float3(shadowUV, 0.0f), shadowZ,
                                         DL_ArrayIndex, DL_Bias);
    return vis;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: IBL indirect probe (tetrahedral interpolation)
//   DXIL: TetraCoordinate + IBLCubemapBVHSRV + CubemapSRV
// ─────────────────────────────────────────────────────────────────────────────

float3 SampleIBLProbe(float3 dir, float roughness)
{
    float mipLevel = IBLDMTInfo_IBLRouhness;
    return CubemapSRV.SampleLevel(TrilinearMirror, dir, mipLevel).rgb;
}

// ─────────────────────────────────────────────────────────────────────────────
// Main: calcShading
// ─────────────────────────────────────────────────────────────────────────────

[numthreads(64, 1, 1)]
void calcShading(uint3 DTid : SV_DispatchThreadID)
{
    uint threadId = DTid.x;

    // ── Bounds check: exit if threadId >= total active strand count ──
    if (threadId >= constant32Bits)
    {
        RWShade[threadId] = 0u;
        return;
    }

    // ── Load compact line index → strandIndex, segmentIndex ──
    uint packedIdx  = CompactLineIndices[threadId];   // from Visibility-filtered list
    uint strandMisc = packedIdx;
    uint flags      = strandMisc >> 28;
    uint strandIdx  = strandMisc & 0x0FFFFFFFu;

    // Check visibility bitmask (1 bit per strand in 32-bit words)
    uint wordIdx  = threadId >> 5;
    uint bitIdx   = threadId & 31u;
    uint visWord  = Visibility[wordIdx];
    if ((visWord & (1u << bitIdx)) == 0u)
    {
        RWShade[threadId] = 0u;
        return;
    }

    // ── Load vertex data for this segment ──
    LineVertices v0 = Vertices[strandIdx];
    LineVertices v1 = Vertices[strandIdx + 1];

    float3 P0 = v0.p;   // current segment start
    float3 P1 = v1.p;   // current segment end
    float3 midP = (P0 + P1) * 0.5f;  // segment midpoint (shading position)

    // Tangent T: compute from neighbors if available, otherwise finite-diff
    float3 T = float3(0, 0, 0);
    bool hasPrev = ((flags & 1u) == 0u);
    if (hasPrev)
    {
        LineVertices vPrev = Vertices[strandIdx - 1];
        float3 dPrev = P0 - vPrev.p;
        T += normalize(dPrev);
    }
    bool hasNext = ((flags & 2u) == 0u);
    if (hasNext)
    {
        LineVertices vNext = Vertices[strandIdx + 2];
        float3 dNext = vNext.p - P1;
        T += normalize(dNext);
    }
    T = normalize(T);

    // ── Decode width and tip blend from misc bits ──
    uint  miscBits  = v0.misc;
    float widthRoot = (float)(miscBits >> 24) * (1.0f / 254.0f);  // normalized root width
    float widthTip  = (float)((miscBits >> 16) & 0xFFu) * (1.0f / 254.0f); // normalized tip width

    // ── Camera direction ──
    // Camera world pos from transposeViewInvMat column 3
    float3 camPos = float3(transposeViewInvMat[0].w,
                           transposeViewInvMat[1].w,
                           transposeViewInvMat[2].w);
    float3 V = normalize(camPos - midP);

    // ── Build hair frame: T, N (perp to T in T-V plane), B ──
    float3 toView = normalize(camPos - P0);
    float3 B = normalize(cross(T, toView));
    B = normalize(B - dot(B, T) * T);
    float3 N = cross(T, B);

    // ── Screen-space NDC for light culling lookup ──
    float4 clipPos = mul(viewProjMat, float4(midP, 1.0f));
    float2 screenUV = clipPos.xy / clipPos.w;
    float2 screenNormUV = screenUV * float2(0.5f, -0.5f) + 0.5f;

    // ── Tip blend parameter ──
    //  Smooth-step between root-roughness and tip-roughness based on widthTip
    float tipBlend;
    {
        float minW = hm_minWidth;
        float maxW = hm_maxWidth;
        float safeRange = (minW == maxW) ? (minW + 1e-8f) : maxW;
        float t = saturate((widthTip - minW) / (safeRange - minW));
        tipBlend = t * t * (3.0f - 2.0f * t);   // smoothstep
    }

    // ── Effective sigma (absorption) with tip blend + random variation ──
    float3 sigmaRoot = hm_sigma;
    float3 sigmaTip  = hm_sigmaTip;

    // Random per-strand variation (packed in misc)
    // DXIL loads from cbuffer UserMaterial (VAR_ColorA/B/C/ColorPoint) for color palette
    // then blends via HSV offsets. Simplified here as identity.
    float3 sigmaEff = lerp(sigmaRoot, sigmaTip, tipBlend);

    // ── Compute albedo from sigma (for multi-scatter) ──
    float3 absorb = SigmaToAlbedo(sigmaEff);     // absorb = exp(-sigma) proxy
    float3 absorb_TT  = exp(-2.0f * sigmaEff);
    float3 absorb_TRT = exp(-4.0f * sigmaEff);

    // ── Lut_Ntt sample: Texture2D<float2>, .x=N_r, .y=N_tt ──
    //   coord0 = dot(T, L) (computed per-light below)
    //   coord1 = hm_sqrt_roughness (beta axis, saturated)
    float lutV = saturate(hm_sqrt_roughness);

    // ── Marschner longitudinal lobes (M-terms) ──
    // sinθ of hair relative to view direction
    float sinThetaO = dot(T, V);   // sinθ_o (outgoing)
    float cosThetaO = sqrt(max(0.0f, 1.0f - sinThetaO * sinThetaO));

    // Roughness parameters
    float betaR   = hm_sqrt_roughness;
    float betaTT  = hm_sqrt_v;
    float betaTRT = hm_sqrt_s;

    // Cuticle tilt shifts
    float alpha = hm_cuticleTiltAngle;
    float sinAlpha = sin(alpha);
    float cosAlpha = cos(alpha);

    // ── Deep Shadow Volume visibility ──
    DSVolumeInfo dsInfo = DSVolumeInfoBuffer[0];
    float3 dsTransmit = float3(1, 1, 1);
    {
        float3 uvwDS = (midP - dsInfo.mMinAABB) * dsInfo.mInvLength;
        if (all(uvwDS >= 0.0f) && all(uvwDS <= 1.0f))
        {
            int3  coordDS = (int3)(uvwDS * (float3)dsInfo.mResolution);
            // sample with mipLevel = DSInfo_VolumeTracingOffsetScale encoded level
            uint  mipDS   = 0u;
            uint  packed0 = DSVolumeTexture.Load(int4(coordDS, mipDS));
            uint  dens0   = packed0 & 0x00FFFFFFu;
            float den0    = (float)dens0 / 16777215.0f;
            dsTransmit = exp(-2.0f * sigmaEff * den0 * 100.0f);
        }
    }

    // ── Accumulated shading ──
    float3 shadeAccum = float3(0, 0, 0);

    // ════════════════════════════════════════════════════════════════════════
    // DIRECTIONAL LIGHT
    // ════════════════════════════════════════════════════════════════════════
    if (DL_Enable != 0u)
    {
        float3 L = normalize(-DL_Direction);
        float  sinThetaI = dot(T, L);
        float  cosThetaI = sqrt(max(0.0f, 1.0f - sinThetaI * sinThetaI));

        // Sample Lut_Ntt: .x=N_r, .y=N_tt
        float2 lutNtt    = Lut_Ntt.SampleLevel(BilinearClamp, float2(sinThetaI, lutV), 0);
        float  N_r_lut   = lutNtt.x;
        float  N_tt_lut  = lutNtt.y;

        // Marschner M-lobes
        float M_R   = HairM(sinThetaO, sinThetaI, betaR);
        float M_TT  = HairM(sinThetaO, sinThetaI, betaTT);
        float M_TRT = HairM(sinThetaO, sinThetaI, betaTRT);

        // Fresnel
        float F_R  = FresnelSchlick(cosThetaI, hm_f0);
        float F_TT = (1.0f - FresnelSchlick(cosThetaI, hm_f0));
        float T_tt = F_TT * F_TT;

        // Single scattering BSDF lobes
        float3 lobe_R   = M_R  * N_r_lut  * F_R;
        float3 lobe_TT  = M_TT * N_tt_lut * T_tt * absorb_TT;
        float3 lobe_TRT = M_TRT * (absorb_TRT * (1.0f - F_R) * (1.0f - F_R));

        float3 bsdf = lobe_R + lobe_TT + lobe_TRT;

        // Deep shadow + regular shadow
        float dlShadow = DirectionalShadow(midP, clipPos.w);
        float3 dlColor = DL_Color * dlShadow;

        shadeAccum += bsdf * dlColor * dsTransmit;

        // DSLUT multi-scatter (IBL pass over deep shadow)
        float3 msLut = HairMultiScatterDSLUT(sinThetaI, betaR, sigmaEff,
                                              M_R, M_TT, M_TRT);
        shadeAccum += msLut * dlColor * absorb * 0.5f;
    }

    // ════════════════════════════════════════════════════════════════════════
    // PUNCTUAL LIGHTS (forward list, up to hm_guideShadingQualityLightMaxNum)
    // ════════════════════════════════════════════════════════════════════════
    uint maxLights = min(PunctualLightFowardCount, hm_guideShadingQualityLightMaxNum);
    for (uint li = 0u; li < maxLights; li++)
    {
        LightParameter lp = LightParameterSRV[li];

        float3 toLight = lp.position - midP;
        float  distSq  = dot(toLight, toLight);
        if (distSq > lp.boundingRadius * lp.boundingRadius) continue;

        float  dist    = sqrt(distSq);
        float3 L       = toLight / dist;

        // Attenuation
        float atten = 1.0f / max(distSq, 1e-4f);
        // lp.attenuation.xyz = (1, dist, dist²) coefficients
        float denomAtten = dot(lp.attenuation.xyz, float3(1.0f, dist, distSq));
        atten = 1.0f / max(denomAtten, 1e-4f);
        atten *= max(0.0f, 1.0f - pow(dist / lp.boundingRadius, lp.falloff));

        // IES profile
        if (lp.iesId != 0u)
        {
            float angleCos = dot(-L, lp.direction);
            float angle    = acos(clamp(angleCos, -1.0f, 1.0f)) / 3.14159265f;
            atten *= SampleIESTable(lp.iesId, angle);
        }

        // Shadow
        float shadowVis = 1.0f;
        if (lp.shadowIndex != 0xFFFFFFFFu)
            shadowVis = 1.0f - SamplePunctualShadow(midP, lp.shadowIndex);

        float sinThetaI = dot(T, L);
        float2 lutNttP  = Lut_Ntt.SampleLevel(BilinearClamp, float2(sinThetaI, lutV), 0);
        float  N_r_P    = lutNttP.x;
        float  N_tt_P   = lutNttP.y;

        float M_R_P   = HairM(sinThetaO, sinThetaI, betaR);
        float M_TT_P  = HairM(sinThetaO, sinThetaI, betaTT);
        float M_TRT_P = HairM(sinThetaO, sinThetaI, betaTRT);

        float cosThetaI_P = sqrt(max(0.0f, 1.0f - sinThetaI * sinThetaI));
        float F_R_P  = FresnelSchlick(cosThetaI_P, hm_f0);
        float F_TT_P = (1.0f - F_R_P);

        float3 bsdf_P = M_R_P  * N_r_P  * F_R_P
                      + M_TT_P * N_tt_P * F_TT_P * F_TT_P * absorb_TT
                      + M_TRT_P * absorb_TRT * (1.0f - F_R_P) * (1.0f - F_R_P);

        shadeAccum += bsdf_P * lp.color * atten * shadowVis;
    }

    // ════════════════════════════════════════════════════════════════════════
    // IBL (indirect) — tetrahedral light probe
    // ════════════════════════════════════════════════════════════════════════
    {
        float3 iblDir = T;  // hair tangent direction for IBL integration
        float3 iblColor = SampleIBLProbe(iblDir, hm_sqrt_roughness);

        // Scale by indirect multiplier
        iblColor *= IBLDMTInfo_IndirectLightingMultiplier;

        // Ambient BRDF lookup (AmbientBRDF: Texture2D<float3>)
        float2 ambUV = float2(saturate(abs(dot(T, V))), saturate(hm_sqrt_roughness));
        float3 ambBRDF = AmbientBRDF.SampleLevel(BilinearClamp, ambUV, 0);

        shadeAccum += absorb * ambBRDF * iblColor;
    }

    // ════════════════════════════════════════════════════════════════════════
    // BACKSCATTER
    // ════════════════════════════════════════════════════════════════════════
    {
        float backAtten = hm_backscatterScale;
        float sinThetaBack = dot(T, -V);
        float2 lutNttBack = Lut_Ntt.SampleLevel(BilinearClamp,
                                float2(sinThetaBack, lutV), 0);
        // backscatter is a secondary TT path, use N_tt channel
        float3 backColor = lutNttBack.y * absorb_TT * backAtten;
        shadeAccum += backColor;
    }

    // ════════════════════════════════════════════════════════════════════════
    // OUTPUT: pack RGB10A2 or R11G11B10 into uint
    //   DXIL uses InterlockedAdd / store to RWShade as uint
    //   Encoding: R8G8B8A8_UNORM packed (multiply by 255, clamp, pack)
    // ════════════════════════════════════════════════════════════════════════
    float3 finalColor = shadeAccum * hm_guideShadingAdjustShading;
    finalColor = saturate(finalColor);

    // Luminance-based alpha (for depth-write threshold)
    float luma = dot(finalColor, float3(0.2126f, 0.7152f, 0.0722f));
    float alpha = saturate(luma / max(hm_depthwriteWithAlphaBlendingThrehold, 1e-5f));

    uint r = (uint)(finalColor.r * 255.0f + 0.5f);
    uint g = (uint)(finalColor.g * 255.0f + 0.5f);
    uint b = (uint)(finalColor.b * 255.0f + 0.5f);
    uint a = (uint)(alpha        * 255.0f + 0.5f);
    uint packed = (r) | (g << 8) | (b << 16) | (a << 24);

    RWShade[threadId] = packed;
}
