// Calculate hair vertex shading data

#include "CommonCS.csh"
#include "HairBsdf.csh"

cbuffer ShadingLightData
{
    float4 DirectionLightDir;
    float4 DirectionLightColor;
    float3 HairColor;
    float  HairRoughness;
    // HairAlpha: hair cuticle tilt angle (radians).
    // Typical value ≈ 0.07 (~4°, matching Marschner shift convention).
    // Maps to: R shift = -HairAlpha, TT = -HairAlpha*0.5, TRT = +HairAlpha*1.5
    float  HairAlpha;
    float  HairUseRefMarschner;
    float  HairEnableMultiScattering;
    float  HairEnableDeepShadowScattering;
};

struct HairVertexData
{
    float3 Pos;
    int    Misc;
};
StructuredBuffer<HairVertexData> VerticesDatas;
StructuredBuffer<uint>           IdxData;

StructuredBuffer<uint>           LineVisibilityBuffer;

RWStructuredBuffer<uint>         OutHairVertexShadeData;

// DSLut3D: 3D dual-scattering LUT (HairStrandsLUT.csh, PERMUTATION_LUT_TYPE_DUALSCATTERING)
//   UV = (|sin θ_i|, roughness, absorption_ch)
//   .x = A_front (front hemisphere average scattering)
//   .y = A_back  (back hemisphere average scattering)
//   .z = 0, .w = 1 (unused)
// The R/TRT Fresnel and azimuthal terms are evaluated analytically in CSMain
// (the LUT integrates phi out, so it cannot provide an azimuthal distribution).
Texture3D<float4>  DSLut3D;
SamplerState       DSLut3D_sampler;

// DSLutNTT: 2D reparameterized TT azimuthal LUT
//   UV = (theta_o/(PI/2), roughness)
//   .x = a (peak amplitude), .y = b (gaussian falloff)
Texture2D<float4>  DSLutNTT;
// (reuses DSLut3D_sampler – same linear + clamp settings)

// DSVolumeTexture: 3D deep-shadow volume (R32_UINT).
//   Per voxel packed value:
//     low 24 bits = accumulated hair density
//     top  8 bits = coverage/opacity  (coverage = ((v>>24)/255))
//
// Ray-march ported 1:1 from hair_shade_result1.hlsl / hair_shade.dxil:
//   - Clip the shaded-point -> light ray against the volume AABB.
//   - March in voxel-sized steps; per step pick a mip level from the marched
//     distance (mip = min(round(log2(dist/VoxelWorldSize)), 5)) and Load the
//     mip pyramid at coord >> mip.
//   - Accumulate density (weighted by the per-step voxel run length) and track
//     max coverage; stop when coverage saturates.
//   - Combine into per-channel transmittance:
//       T = (sig^2 - sqrt(sig)) * (1 - coverage) * exp(-max(D - 1, 0)) + sqrt(sig)
cbuffer DSVolumeInfo
{
    float4 DSV_mMinAABB;         // [0] xyz
    float4 DSV_mMaxAABB;         // [1] xyz
    uint4  DSV_mResolution;      // [2] xyz
    uint4  DSV_mClearResolution; // [3] xyz
    float4 DSV_mScale;           // [4] xyz = coord remap scale
    float4 DSV_mInvLength;       // [5] xyz = 1/(max-min)
    float4 DSV_mInvResolution;   // [6] xyz = 1/res
};
Texture3D<uint>    DSVolumeTexture;

// DSInfo cbuffer (matches GenerateDSVolumeTexture.csh layout).
//   Row0.x = VoxelWorldSize
//   Row1.x = VolumeTracingOffsetScale (start offset in voxels)
//   Row1.w = VolumeTracingIBLDelta (fallback per-step growth factor)
//   Row2.x = VolumeTracingDelta (per-step growth factor used by DSVolumeTex3D path)
//   Row2.y = VolumePageResolution (max coarse step clamp)
//   Row3.x = RasterDepthThreshold (used by DS volume generation)
//   Row3.y = BackscatterScale (multiple-scattering glow amount, small ~0.1)
cbuffer DSInfo
{
    float4 DSInfo_Row0;   // x=VoxelWorldSize y=VolumeResolution z=VolumePageResolution w=RasterDepthThreshold
    float4 DSInfo_Row1;   // x=LUTThetaCount y=LUTRoughnessCount z=LUTAbsorptionCount w=VolumeTracingOffsetScale
    float4 DSInfo_Row2;   // x=VolumeTracingDelta y=RasterStrandWidthScale z=StrandWidthMin w=StrandWidthMax
    float4 DSInfo_Row3;   // x=StrandWidthAve y=VolumeTracingIBLDelta z=BackscatterScale
};

static const float DSV_DENSITY_MUL = 0.00100000005f; // DSVolumeTex3D density -> optical depth
static const float DSV_MAX_MIP     = 5.0f;        // 6-level pyramid: 128..4

// DeepShadowScattering: ports the DSVolume ray-march of hair_shade.hlsl
// (directional-light path, lines 803..1059 of the dxil-spirv dump).
//
// The r32ui volume stores hair OPACITY only:
//   low 24 bits = accumulated hair density (n = number of hairs toward light)
//   top  8 bits = head/body scattering-occlusion coverage
//
// Ray-shoot from the shading point toward the light and produce three outputs:
//   OutHairCount = n        -> dual scattering (a_f^n, sigma_f^2 = beta_f^2*max(1,n))
//   OutCoverage             -> saturate(1 - coverage) is the actual shadow term
//   return value            -> scattering weight applied to the TT lobe and to the
//                              multiple-scattering lobe ONLY (never to R/TRT):
//     sig = BackscatterScale
//     T   = (sig^2 - sqrt(sig)) * saturate(1 - coverage) * exp(-max(n - 1, 0)) + sqrt(sig)
float3 DeepShadowScattering(float3 worldPos, out float OutHairCount, out float OutCoverage)
{
    float  voxelWorldSize = DSInfo_Row0.x;
    float  offsetScale    = DSInfo_Row1.w;   // VolumeTracingOffsetScale
    float  stepGrowth     = max((DSInfo_Row2.x > 0.0f) ? DSInfo_Row2.x : DSInfo_Row3.y, 1.0f);
    float  pageResolution = max(DSInfo_Row0.z, 1.0f); // VolumePageResolution
    float  backscatter    = DSInfo_Row3.z;   // BackscatterScale (Material.hm_backscatterScale)

    // March direction = toward the light source.
    float3 dirW = normalize(DirectionLightDir.xyz);
    float3 rayStart = worldPos + dirW * voxelWorldSize * offsetScale;

    // --- AABB slab clip: find [t0, t1] intersection of the ray with the volume ---
    float3 invDir = 1.0f / dirW;
    float3 tA = (DSV_mMinAABB.xyz - rayStart) * invDir;
    float3 tB = (DSV_mMaxAABB.xyz - rayStart) * invDir;
    float3 tMin3 = min(tA, tB);
    float3 tMax3 = max(tA, tB);
    float  t0 = max(max(tMin3.x, tMin3.y), tMin3.z);
    float  t1 = min(min(tMax3.x, tMax3.y), tMax3.z);
    t0 = max(t0, 0.0f);

    // Reference phi defaults when the ray never marches a voxel:
    //   accumDensity = 0.5, coverage = 0.
    float  accumDensity = 0.5f;
    float  maxCoverage  = 0.0f;

    if (t0 < t1)
    {
        // Entry point + quantized voxel-sized marching.
        float3 entry    = rayStart + dirW * t0;
        float  segLen   = min((t1 - t0), 1e5f);
        float3 stepDir  = dirW * voxelWorldSize;                 // one unit ~ one voxel
        float  numSteps = ceil(segLen / max(voxelWorldSize, 1e-6f));
        float  baseStep = segLen / max(numSteps, 1.0f);

        if (numSteps > 0.0f)
        {
            accumDensity = 0.0f;
            int3   prevCoord = int3(-1, -1, -1);
            float  curStep   = 1.0f;   // grows with distance, weights density
            float  marched   = 0.0f;   // accumulated voxel-count along the ray

            float3 res_1  = (float3)(DSV_mResolution.xyz - uint3(1u, 1u, 1u));

            [loop]
            for (uint s = 0u; marched < numSteps; ++s)
            {
                float3 p   = entry + stepDir * marched;

                // uvw must mirror the write mapping in GenerateDSVolumeTexture.csh:
                //   tc = saturate((wp - min) * invLength); tc.y/z = 1 - tc.y/z;
                //   voxel = tc * (mResolution - 1)
                float3 uvw = saturate((p - DSV_mMinAABB.xyz) * DSV_mInvLength.xyz);
                uvw.y = 1.0f - uvw.y;
                uvw.z = 1.0f - uvw.z;

                int3 coord = (int3)(uvw * res_1);

                if (all(coord == prevCoord))
                {
                    marched += curStep;
                    continue;
                }
                prevCoord = coord;

                // Mip selection from marched distance.
                float mipRaw = (curStep * baseStep) / max(voxelWorldSize, 1e-6f);
                uint  mip    = (uint)min(max(round(log2(max(mipRaw, 1e-6f))), 0.0f), DSV_MAX_MIP);
                uint  mipSh  = mip & 31u;

                int3 mipCoord = int3((uint3)coord >> mipSh);
                uint packed   = DSVolumeTexture.Load(int4(mipCoord, mip));

                float density  = (float)(packed & 0x00FFFFFFu) * mipRaw;
                float coverage = saturate((float)((packed >> 24u) & 0xFFu) * (1.0f / 255.0f));

                accumDensity += density * DSV_DENSITY_MUL;
                maxCoverage   = max(maxCoverage, coverage);

                if (maxCoverage >= 1.0f)
                    break;

                float nextStep = min(curStep * stepGrowth, pageResolution);
                marched += nextStep;
                curStep  = nextStep;
            }
        }
        else
        {
            accumDensity = 0.0f;
        }
    }

    // Per-channel multiple-scattering weight (faithful reference combine).
    float3 sig     = backscatter.xxx;
    float3 sqrtSig = sqrt(sig);
    float3 sigSq   = sig * sig;
    float  oneMCov = saturate(1.0f - maxCoverage);
    float  falloff = exp(-max(accumDensity - 1.0f, 0.0f));

    OutHairCount = accumDensity;
    OutCoverage  = maxCoverage;
    return (sigSq - sqrtSig) * oneMCov * falloff + sqrtSig;
}

float3 FromLinearAbsorption(float3 In) { return sqrt(In); }

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID,
            uint3 group_id : SV_GroupID,
            uint  group_thread_idx : SV_GroupIndex)
{
    if ((LineVisibilityBuffer[id.x >> 5u] & (1u << (id.x & 31u))) == 0u)
        return;

    // --------------------------------------------------------
    // 1. 解码 strand 索引包（高 4 位 = 类型/标志，低 28 位 = 顶点索引）
    // --------------------------------------------------------
    uint packedInfo  = IdxData[id.x];
    uint strandFlags = packedInfo >> 28u;           // bit0=root, bit1=tip
    uint VertexIdx0  = packedInfo & 0x0FFFFFFFu;

    HairVertexData V0 = VerticesDatas[VertexIdx0];
    if (isnan(V0.Pos.x))
        return;

    HairVertexData V1 = VerticesDatas[VertexIdx0 + 1];

    // --------------------------------------------------------
    // 2. Strand tangent（Marschner 平滑切线：前后段平均）
    // --------------------------------------------------------
    float3 tangPrev = float3(0, 0, 0);
    if ((strandFlags & 1u) == 0u)   // not root: compute backward tangent
    {
        HairVertexData Vprev = VerticesDatas[VertexIdx0 - 1];
        float3 dPrev  = V0.Pos - Vprev.Pos;
        tangPrev      = dPrev * rsqrt(dot(dPrev, dPrev));
    }

    float3 tangNext = float3(0, 0, 0);
    if ((strandFlags & 2u) == 0u)   // not tip: compute forward tangent
    {
        float3 dNext  = V1.Pos - V0.Pos;
        tangNext      = dNext * rsqrt(dot(dNext, dNext));
    }

    float3 tangSum = tangPrev + tangNext;
    float3 T       = tangSum * rsqrt(dot(tangSum, tangSum));   // averaged hair tangent
    float3 V = normalize(CameraWPos.xyz - V1.Pos);           // view vector
    float3 L = normalize(DirectionLightDir.xyz);             // light direction

    // --------------------------------------------------------
    // 3. GBuffer + 双散射预计算（沿用原有 DSLut3D 采样路径）
    // --------------------------------------------------------
    FGBufferData hair_gb;
    hair_gb.BaseColor = HairColor;
    hair_gb.Roughness = HairRoughness;

    // Deep-shadow volume ray-march toward the light: hair count n, body coverage
    // and the scattering weight applied to the TT / multiple-scattering lobes.
    float  ds_hair_count = 0.0f;
    float  ds_coverage   = 0.0f;
    float3 ds_scatter    = DeepShadowScattering(V1.Pos, ds_hair_count, ds_coverage);

    // Compare toggle: when disabled, bypass the deep-shadow contribution so the
    // hair renders with no self-shadow coverage, no TT scatter modulation and no
    // dual-scatter hair count (neutral baseline for A/B comparison).
    if (HairEnableDeepShadowScattering < 0.5f)
    {
        ds_hair_count = 0.0f;
        ds_coverage   = 0.0f;
        ds_scatter    = float3(1.0f, 1.0f, 1.0f);
    }

    float  SinLightAngle       = dot(L, T);
    float3 RemappedAbsorption  = FromLinearAbsorption(HairColor);

    float3 sUV_r = float3(saturate(abs(SinLightAngle)), saturate(HairRoughness), saturate(RemappedAbsorption.x));
    float3 sUV_g = float3(saturate(abs(SinLightAngle)), saturate(HairRoughness), saturate(RemappedAbsorption.y));
    float3 sUV_b = float3(saturate(abs(SinLightAngle)), saturate(HairRoughness), saturate(RemappedAbsorption.z));

    float2 scat_r = DSLut3D.SampleLevel(DSLut3D_sampler, sUV_r, 0).xy;
    float2 scat_g = DSLut3D.SampleLevel(DSLut3D_sampler, sUV_g, 0).xy;
    float2 scat_b = DSLut3D.SampleLevel(DSLut3D_sampler, sUV_b, 0).xy;

    float3 A_front = float3(scat_r.x, scat_g.x, scat_b.x);
    float3 A_back  = float3(scat_r.y, scat_g.y, scat_b.y);

    FHairTransmittanceData TransData = ComputeDualScatteringTerms(
        HairRoughness, V, L, T, A_front, A_back, ds_hair_count);

    // Reference `_2739 = DS * 0.7*PI` scales the local-scattering lobe that sits
    // inside the global-scattering bracket (hair_shade.hlsl:1208-1211).
    TransData.LocalScattering *= ds_scatter * 0.7f;

    // --------------------------------------------------------
    // 4. Marschner 单散射 BSDF
    // --------------------------------------------------------

    // 4.1 纵向角（T 作切线，dot 值 = sinθ in Marschner notation）
    float cosThI  = dot(T, L);      // sinθ_i
    float cosThR  = dot(T, V);      // sinθ_r

    // 4.2 纵向 half-angle → 用于 M 瓣
    float thetaI  = asin(clamp(cosThI, -1.0f, 1.0f));
    float thetaR  = asin(clamp(cosThR, -1.0f, 1.0f));
    float thetaH  = (thetaI + thetaR) * 0.5f;

    // 4.3 三瓣宽度
    float roughSq   = HairRoughness * HairRoughness;
    float betaR_w   = roughSq;
    float betaTT_w  = roughSq * 0.5f;
    float betaTRT_w = roughSq * 2.0f;

    // 4.4 方位角差 φ_d（L 与 V 在法平面的投影夹角）
    //     TT 峰值在 φ_d ≈ π（正向透射）
    float3 Li_perp = L - cosThI * T;
    float3 Lr_perp = V - cosThR * T;
    float  lenLiLr = dot(Li_perp, Li_perp) * dot(Lr_perp, Lr_perp);
    float  cosPhi  = dot(Li_perp, Lr_perp) * rsqrt(max(lenLiLr, 1e-8f));
    float  phi_o   = acos(clamp(cosPhi, -1.0f, 1.0f));   // ∈ [0, π]

    static const float NTT_HALF_PI = 1.5707963f;
    static const float NTT_TWO_PI = 6.2831853f;

    // 4.5 NTT LUT sampling: UV = (theta_o normalized, roughness)
    //     theta_o follows Frostbite presentation parameterization.
    float thetaO = abs(thetaR);
    float2 nttUV = float2(saturate(thetaO / NTT_HALF_PI),
                          saturate(HairRoughness));
    float4 nttSmp   = DSLutNTT.SampleLevel(DSLut3D_sampler, nttUV, 0.0f);
    float  nttA     = max(nttSmp.x, 0.0f);
    float  nttB     = max(nttSmp.y, 0.01f);
    float  dphiTT   = phi_o - PI;
    dphiTT         -= NTT_TWO_PI * round(dphiTT / NTT_TWO_PI); // wrap [-pi, pi]
    float  N_tt_fit = nttA * exp(-nttB * dphiTT * dphiTT);

    // 4.6 A_TT_h0：h=0 近似的 TT 吸收项
    //     eta_p = Bravais 等效折射率（斜入射修正）
    //     F     = Schlick Fresnel @ h=0 (cos γ_i = 1) = R0
    //     T     = exp(-2σ_a / cos γ_t)
    float  eta_hair    = 1.55f;
    float  sinTh_abs   = abs(cosThR);   // |sinθ_r|
    float  cosTh_abs   = sqrt(max(1.0f - sinTh_abs * sinTh_abs, 0.0f));
    float  eta_p       = sqrt(max(eta_hair * eta_hair - sinTh_abs * sinTh_abs, 1e-6f))
                         / max(cosTh_abs, 1e-4f);
    float  R0          = (eta_p - 1.0f) / (eta_p + 1.0f);
    R0                *= R0;            // Schlick R0；h=0 → (1-cosγ_i)^5=0 → F=R0
    float  one_mF      = 1.0f - R0;
    float  cos_gt      = sqrt(max(1.0f - 1.0f / (eta_p * eta_p), 0.0f));
    float3 sigma_a     = -log(max(HairColor, 1e-6f));   // 正值吸收系数
    float3 T_abs       = exp(-2.0f * sigma_a / max(cos_gt, 0.01f));
    float3 A_TT        = one_mF * one_mF * T_abs;       // (1-F)² · T

    // 4.7 R / TRT 的 Fresnel（解析式，取代原先误用的 MEAN_ENERGY LUT 通道）
    //     与 HairBsdf.csh 的 Attenuation() 一致：
    //       p=0 (R)  : F = Hair_F(sqrt(0.5 + 0.5 * dot(V, L)))
    //       p=2 (TRT): F = Hair_F(cosθ_d * sqrt(1 - h²))，h=0 → Hair_F(cosθ_d)
    //     Fresnel 与吸收无关，故三通道同值；钳位 [0, 0.99]
    float  VoL       = dot(V, L);
    float  cosThetaD = cos(0.5f * abs(thetaR - thetaI));
    float3 fresnel0  = min(Hair_F(sqrt(saturate(0.5f + 0.5f * VoL))), 0.99f);
    float3 fresnel1  = min(Hair_F(saturate(cosThetaD)), 0.99f);
    float3 one_f1    = 1.0f - fresnel1;

    // 4.8 各瓣纵向高斯 M
    float shift  = HairAlpha;
    float M_R    = LongitudinalGaussian(thetaH - shift,        betaR_w   * betaR_w);
    float M_TT   = LongitudinalGaussian(thetaH - shift * 0.5f, betaTT_w  * betaTT_w  * 0.5f);
    float M_TRT  = LongitudinalGaussian(thetaH + shift * 1.5f, betaTRT_w * betaTRT_w * 2.0f);

    // 4.9 TRT 吸收（4次透射）
    //     复用 sigma_a 已算好
    float3 T_single = exp(-sigma_a / max(cos_gt, 0.01f));   // 单次穿透
    float3 T_TRT    = T_single * T_single * T_single * T_single;

    // 4.10 方位角分量（解析式，h=0 近似 + 9 项环绕 Gaussian detector）
    //     参考 AzimuthalScattering()：Np(p) ≈ A(p, h=0) * Dp(B[p], φ - Ω(p, 0))
    //     Ω(0, 0) = 0，Ω(2, 0) = 2π；A 已拆到 fresnel0 / fresnel1·T_TRT
    float3 az_R   = GaussianDetector(max(betaR_w,   0.01f), phi_o);
    float3 az_TRT = GaussianDetector(max(betaTRT_w, 0.01f), phi_o - NTT_TWO_PI);
    // TT 方位角：N_tt_fit × A_TT × DS（A_TT 已含 (1-F)^2 和透射衰减）
    // 参考 hair_shade.hlsl:1106-1108 —— DS 只调制 TT 瓣，R / TRT 不受影响。
    float3 az_TT  = N_tt_fit.xxx * A_TT * ds_scatter;

    // 4.11 三瓣 Marschner 单散射
    //   R   : 外表面 Fresnel 反射
    //   TT  : 方位角 D_TT × 吸收 A_TT（h=0 近似，与 LUT 解耦）
    //   TRT : (1-F)² · F · 透射^4 × 方位角权重
    float3 bsdf_R   = M_R   * fresnel0                     * az_R;
    float3 bsdf_TT  = M_TT  * az_TT;
    float3 bsdf_TRT = M_TRT * (one_f1 * one_f1) * fresnel1 * T_TRT * az_TRT;

    float3 marschner_fs_lut = bsdf_R + bsdf_TT + bsdf_TRT;

    // Optional compare path: use HairShadingRef single-scattering instead of LUT-NTT reconstructed marschner_fs.
    uint2 randRef = uint2(id.x, VertexIdx0);
    float3 marschner_fs_ref = HairShadingRef(hair_gb, L, V, T, randRef,
        HAIR_COMPONENT_R | HAIR_COMPONENT_TT | HAIR_COMPONENT_TRT);

    float useRef = step(0.5f, HairUseRefMarschner);
    float3 marschner_fs = lerp(marschner_fs_lut, marschner_fs_ref, useRef);

    // --------------------------------------------------------
    // 5. 双散射包装 + Kajiya-Kay 漫射
    // --------------------------------------------------------
    float3 hair_single_scatter = marschner_fs + KajiyaKayDiffuseAttenuation(hair_gb, L, V, T, 1.0f);
    float3 hair_multi_scatter  = EvaluateHairMultipleScattering(TransData, marschner_fs)
                               + KajiyaKayDiffuseAttenuation(hair_gb, L, V, T, 1.0f);

    float useMulti = step(0.5f, HairEnableMultiScattering);
    float3 hair_dir_fs = lerp(hair_single_scatter, hair_multi_scatter, useMulti);
    hair_dir_fs = max(hair_dir_fs, 0.0f);

    // Directional light contribution, matching hair_shade.hlsl:1209-1211:
    //   L = Attenuation * DL_Color.rgb * cos(theta_i) * saturate(1 - coverage) * [bsdf]
    // (`_2463` = cos(theta_i), `_2213` = saturate(1 - coverage), `_138._m0[5u]` = DL_Color).
    float cosThetaI = sqrt(max(1.0f - cosThI * cosThI, 0.0f));
    hair_dir_fs *= DirectionLightColor.rgb * (saturate(1.0f - ds_coverage) * cosThetaI);

    // --------------------------------------------------------
    // 6. 打包输出
    // --------------------------------------------------------
    OutHairVertexShadeData[VertexIdx0 + 1] = PackR11G11B10F(hair_dir_fs);

    // Copy to current CP if root strand or previous strand inactive
    bool prevInactive = (id.x == 0u) ||
        ((LineVisibilityBuffer[(id.x - 1u) >> 5u] & (1u << ((id.x - 1u) & 31u))) == 0u);
    if ((strandFlags == 1u) || prevInactive)
    {
        OutHairVertexShadeData[VertexIdx0] = PackR11G11B10F(hair_dir_fs);
    }
}
