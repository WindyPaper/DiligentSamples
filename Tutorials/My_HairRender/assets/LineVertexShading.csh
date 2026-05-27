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
//   UV = (0.5 - cosθ_i * 0.5,  sqrt(betaTT))
//   .x = nttA (azimuthal TT coefficient), .y = nttB
Texture2D<float4>  DSLutNTT;
// (reuses DSLut3D_sampler – same linear + clamp settings)

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
    // 4. Marschner 单散射 BSDF（LUT-based，三瓣分通道）
    //    替换原 HairShadingRef 路径
    // --------------------------------------------------------

    // 4.1 纵向角（T 作切线，dot 值 = sinθ in Marschner notation）
    float cosThI  = dot(T, L);      // ≡ sinθ_i  (angle from normal plane)
    float cosThR  = dot(T, V);      // ≡ sinθ_r

    // 4.2 各通道吸收系数 σ = log2(c) * 0.11771371
    float sigmaR = log2(max(HairColor.r, 1e-6f)) * 0.11771371f;
    float sigmaG = log2(max(HairColor.g, 1e-6f)) * 0.11771371f;
    float sigmaB = log2(max(HairColor.b, 1e-6f)) * 0.11771371f;
    float sigRsq = sigmaR * sigmaR;
    float sigGsq = sigmaG * sigmaG;
    float sigBsq = sigmaB * sigmaB;

    // 4.3 三瓣宽度（与 HairBsdf.csh 的 B[] 一致）
    float roughSq  = HairRoughness * HairRoughness;
    float betaR_w  = roughSq;           // R  lobe width
    float betaTT_w = roughSq * 0.5f;    // TT lobe width
    float betaTRT_w= roughSq * 2.0f;    // TRT lobe width

    // 4.4 NTT LUT 采样（TT 方位角，UV = (0.5 - cosThI*0.5, sqrt(betaTT))）
    float2 nttUV  = float2(0.5f - cosThI * 0.5f, sqrt(betaTT_w));
    float4 nttSmp = DSLutNTT.SampleLevel(DSLut3D_sampler, nttUV, 0.0f);
    float  nttA   = nttSmp.x;   // azimuthal TT coefficient

    // 4.5 3D LUT 采样（Fresnel 权重 + 方位角 R/TRT，按各瓣 beta 分通道）
    float cosThAbs = saturate(abs(cosThI));
    float4 lut3dR  = DSLut3D.SampleLevel(DSLut3D_sampler,
        float3(cosThAbs, saturate(betaR_w),   saturate(sqrt(max(sigmaR, 0.0f)))), 0.0f);
    float4 lut3dG  = DSLut3D.SampleLevel(DSLut3D_sampler,
        float3(cosThAbs, saturate(betaTT_w),  saturate(sqrt(max(sigmaG, 0.0f)))), 0.0f);
    float4 lut3dBv = DSLut3D.SampleLevel(DSLut3D_sampler,
        float3(cosThAbs, saturate(betaTRT_w), saturate(sqrt(max(sigmaB, 0.0f)))), 0.0f);

    // Fresnel 钳位 [0, 0.99]
    float3 fresnel0 = float3(min(lut3dR.x,  0.99f), min(lut3dG.x,  0.99f), min(lut3dBv.x, 0.99f));
    float3 fresnel1 = float3(min(lut3dR.y,  0.99f), min(lut3dG.y,  0.99f), min(lut3dBv.y, 0.99f));
    float3 one_f0   = 1.0f - fresnel0;

    // 4.6 纵向角转换 → half-angle θ_H
    float thetaI = asin(clamp(cosThI, -1.0f, 1.0f));
    float thetaR = asin(clamp(cosThR, -1.0f, 1.0f));
    float thetaH = (thetaI + thetaR) * 0.5f;

    // 4.7 各瓣纵向高斯 M（均值偏移对齐 HairShadeCS.csh 约定）
    float shift    = HairAlpha;                   // >0, e.g. 0.07
    float M_R   = LongitudinalGaussian(thetaH - shift,          betaR_w  * betaR_w);
    float M_TT  = LongitudinalGaussian(thetaH - shift * 0.5f,   betaTT_w * betaTT_w * 0.5f);
    float M_TRT = LongitudinalGaussian(thetaH + shift * 1.5f,   betaTRT_w* betaTRT_w * 2.0f);

    // 4.8 各通道吸收衰减（exp2 形式）
    float absR = exp2(sigRsq * (-1.44269502f));
    float absG = exp2(sigGsq * (-1.44269502f));
    float absB = exp2(sigBsq * (-1.44269502f));

    float3 T_TT  = float3(absR * absR,    absG * absG,    absB * absB);    // TT:  (1-pass)^2
    float3 T_TRT = float3(T_TT.r * T_TT.r, T_TT.g * T_TT.g, T_TT.b * T_TT.b); // TRT: (1-pass)^4

    // 4.9 方位角分量（LUT3D .z=R, .w=TRT；NTT for TT）
    float3 az_R   = float3(lut3dR.z,   lut3dG.z,   lut3dBv.z);
    float3 az_TRT = float3(lut3dR.w,   lut3dG.w,   lut3dBv.w);
    float3 az_TT  = float3(nttA * absR, nttA * absG, nttA * absB);

    // 4.10 三瓣分通道 Marschner 单散射
    //   R   : Fresnel 反射
    //   TT  : 透射^2 × 方位角（透过纤维）
    //   TRT : 内部反射 × 透射^4 × 方位角
    float3 bsdf_R   = M_R   * fresnel0             * az_R;
    float3 bsdf_TT  = M_TT  * (one_f0 * one_f0)   * T_TT  * az_TT;
    float3 bsdf_TRT = M_TRT * (one_f0 * one_f0)   * fresnel1 * T_TRT * az_TRT;

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
