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
    float3 _pad;
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

// DSLut3D: 3D dual-scattering LUT
//   UV = (|sin θ_i|, roughness, absorption_ch)
//   .x = A_front, .y = A_back,
//   .z = azimuthal R weight, .w = azimuthal TRT weight
Texture3D<float4>  DSLut3D;
SamplerState       DSLut3D_sampler;

// DSLutNTT: 2D azimuthal NTT LUT  (used for TT lobe)
//   UV = (x-mode(sinThetaI), roughness)
//   .x = N_r, .y = N_tt
Texture2D<float4>  DSLutNTT;
// (reuses DSLut3D_sampler – same linear + clamp settings)

#define NTT_X_COORD_SIGNED_REMAP 0
#define NTT_X_COORD_ABS_SAT      1
#define NTT_X_COORD_DIRECT_01    2

#ifndef NTT_X_COORD_MODE
// Keep same default as HairStrandsLUT.csh
#define NTT_X_COORD_MODE NTT_X_COORD_SIGNED_REMAP
#endif

float EncodeNttLutX(float sinThetaI)
{
#if NTT_X_COORD_MODE == NTT_X_COORD_SIGNED_REMAP
    return saturate(sinThetaI * 0.5f + 0.5f);
#elif NTT_X_COORD_MODE == NTT_X_COORD_ABS_SAT
    return saturate(abs(sinThetaI));
#else // NTT_X_COORD_DIRECT_01
    return saturate(sinThetaI);
#endif
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
        HairRoughness, V, L, T, A_front, A_back);

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

    // 4.5 NTT LUT sampling: UV = (sinThetaI remapped, roughness)
    //     Must match HairStrandsLUT.csh default mode:
    //       NTT_X_COORD_SIGNED_REMAP -> x = sinThetaI * 0.5 + 0.5
    float2 nttUV    = float2(EncodeNttLutX(cosThI),
                             saturate(HairRoughness));
    float4 nttSmp   = DSLutNTT.SampleLevel(DSLut3D_sampler, nttUV, 0.0f);
    float  N_r_lut  = nttSmp.x;
    float  N_tt_lut = nttSmp.y;

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

    // 4.7 3D LUT 采样（双散射权重 A_front/A_back 已在第3步用过；
    //     这里复用 MEAN_ENERGY 通道的 Fresnel/方位角权重给 R/TRT）
    //     各通道对应不同 beta（R / TRT 分别用自己的宽度）
    float  cosThAbs    = saturate(abs(cosThI));
    float4 lut3dR  = DSLut3D.SampleLevel(DSLut3D_sampler,
        float3(cosThAbs, saturate(betaR_w),
               saturate(RemappedAbsorption.r)), 0.0f);
    float4 lut3dG  = DSLut3D.SampleLevel(DSLut3D_sampler,
        float3(cosThAbs, saturate(betaR_w),
               saturate(RemappedAbsorption.g)), 0.0f);
    float4 lut3dBv = DSLut3D.SampleLevel(DSLut3D_sampler,
        float3(cosThAbs, saturate(betaR_w),
               saturate(RemappedAbsorption.b)), 0.0f);

    // Fresnel 钳位 [0, 0.99]
    float3 fresnel0 = float3(min(lut3dR.x,  0.99f),
                             min(lut3dG.x,  0.99f),
                             min(lut3dBv.x, 0.99f));
    float3 fresnel1 = float3(min(lut3dR.y,  0.99f),
                             min(lut3dG.y,  0.99f),
                             min(lut3dBv.y, 0.99f));
    float3 one_f0   = 1.0f - fresnel0;

    // 4.8 各瓣纵向高斯 M
    float shift  = HairAlpha;
    float M_R    = LongitudinalGaussian(thetaH - shift,        betaR_w   * betaR_w);
    float M_TT   = LongitudinalGaussian(thetaH - shift * 0.5f, betaTT_w  * betaTT_w  * 0.5f);
    float M_TRT  = LongitudinalGaussian(thetaH + shift * 1.5f, betaTRT_w * betaTRT_w * 2.0f);

    // 4.9 TRT 吸收（4次透射）
    //     复用 sigma_a 已算好
    float3 T_single = exp(-sigma_a / max(cos_gt, 0.01f));   // 单次穿透
    float3 T_TRT    = T_single * T_single * T_single * T_single;

    // 4.10 方位角分量
    float3 az_R   = N_r_lut.xxx;
    float3 az_TRT = float3(lut3dR.w,  lut3dG.w,  lut3dBv.w);
    // TT 方位角：N_tt × A_TT（A_TT 已含 (1-F)^2 和透射衰减）
    float3 az_TT  = N_tt_lut.xxx * A_TT;

    // 4.11 三瓣 Marschner 单散射
    //   R   : 外表面 Fresnel 反射
    //   TT  : 方位角 D_TT × 吸收 A_TT（h=0 近似，与 LUT 解耦）
    //   TRT : 内反射 × 透射^4 × 方位角权重
    float3 bsdf_R   = M_R   * fresnel0                       * az_R;
    float3 bsdf_TT  = M_TT  * az_TT;
    float3 bsdf_TRT = M_TRT * (one_f0 * one_f0) * fresnel1 * T_TRT * az_TRT;

    float3 marschner_fs = bsdf_R + bsdf_TT + bsdf_TRT;

    // --------------------------------------------------------
    // 5. 双散射包装 + Kajiya-Kay 漫射
    // --------------------------------------------------------
    float3 hair_dir_fs = EvaluateHairMultipleScattering(TransData, marschner_fs);
    hair_dir_fs       += KajiyaKayDiffuseAttenuation(hair_gb, L, V, T, 1.0f);
    hair_dir_fs        = max(hair_dir_fs, 0.0f);

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
