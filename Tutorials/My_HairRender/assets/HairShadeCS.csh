// HairShadeCS.csh
// Compute Shader: Marschner hair shading per strand control-point.
// Ported from G:/git_projects/dxil-spirv/hair_shade.hlsl (GLSL->HLSL).
// [numthreads(64, 1, 1)]

#pragma once
#include "HairShadingCommon.csh"

// ============================================================
// Constant Buffers
// ============================================================

// b0 – Camera / View / Shadow matrices  (33 float4)
cbuffer CB_ViewShadow : register(b0, space0)
{
    float4 cb0[33];
    // [0..2]  = ViewProj rows 0-2 (xyz), [3]=VP w-col (xyz)
    // [7..9]  .w = camPos.xyz
    // [23].xy = viewport width/height (pixels)
    // [32].z  = active light feature flags mask
};

// b1 – Lighting & Voxel params  (31 float4)
cbuffer CB_Lighting : register(b1, space0)
{
    float4 cb1[31];
    // [3].xy   = PCF texel size (1/shadowMapWidth, 1/shadowMapHeight)
    // [4].xyz  = dir-light world position * 1e9 (encodes direction + distance)
    // [4].w    = dir-light active flag (byte-packed, bit 0..7)
    // [5].xyz  = dir-light color (linear)
    // [12..15] = voxel->shadow transform rows 0-3
    // [16].y   = cascade index (uint bits), [16].w = cascade depth offset
    // [18].xy  = scaled shadow UV offset, [18].z = shadow UV scale
    // [21].x   = higher-cascade depth scale
};

// b2 – Voxel / Cluster  (8 float4)
cbuffer CB_Voxel : register(b2, space0)
{
    float4 cb2[8];
};

// b3 – Per-object / Strand  (10 float4)
cbuffer CB_PerObject : register(b3, space0)
{
    float4 cb3[10];
    // [0].w    = IOR eta_prime (refracted index)
    // [1].x    = alpha tilt angle (radians)
    // [1].y    = betaTT^2 (longitudinal variance TT)
    // [1].z    = betaR^2  (longitudinal variance R)
    // [1].w    = betaTRT^2(longitudinal variance TRT)
    // [2].x    = Fresnel F0 / fibre thickness factor
    // [5].w    = root u-shift, [4].w = tip u-shift
    // [6].w    = hair roughness (longitudinal variance override)
    // [6].w    = (reused) shadow weight for BSDF
};

// b4 – Misc (4 float4)
cbuffer CB_Misc : register(b4, space0)
{
    float4 cb4[4];
};

// b5 – Color / Tone  (3 float4)
cbuffer CB_Color : register(b5, space0)
{
    float4 cb5[3];
};

// b6 – Hair material params  (6 float4)
cbuffer CB_HairMat : register(b6, space0)
{
    float4 cb6[6];
    // [0].xyz = base hair color (sRGB)
    // [1].xyz = specular color tint (sRGB)
    // [2].xyz = third ramp color (sRGB)
    // [3]     = u-range (minU1, maxU1, minU2, maxU2) for dual-stage ramp
    // [4]     = hue-shift params (scale, uShift, satClamp, valShift)
    // [5].x   = saturation clamp low, [5].y = value clamp low
};

// b0 (space32) – Dispatch params
cbuffer CB_Dispatch : register(b0, space32)
{
    float4 cb_dispatch[1];  // [0].x = active strand count (uint bits via asuint)
};

// ============================================================
// Structured / Byte Address Buffers
// ============================================================

ByteAddressBuffer   buf_strandIndex  : register(t0, space0);  // strand index -> cpBase (28-bit) + flags (4-bit)
ByteAddressBuffer   buf_strandCP     : register(t1, space0);  // control-point positions (stride=4 uint: float3 pos + packed attr)
ByteAddressBuffer   buf_activeMask   : register(t2, space0);  // 1-bit-per-strand active bitmask

ByteAddressBuffer   buf_voxelList_u1 : register(t3, space0);
Buffer<uint2>       buf_voxelList_u2 : register(t4, space0);
Buffer<uint4>       buf_voxelList_u4 : register(t5, space0);

ByteAddressBuffer   buf_lights_u1    : register(t6, space0);
Buffer<uint2>       buf_lights_u2    : register(t7, space0);
Buffer<uint4>       buf_lights_u4    : register(t8, space0);

ByteAddressBuffer   buf_cluster_u1   : register(t9,  space0);
Buffer<uint2>       buf_cluster_u2   : register(t10, space0);
Buffer<uint4>       buf_cluster_u4   : register(t11, space0);

ByteAddressBuffer   buf_clusterMask  : register(t12, space0);
ByteAddressBuffer   buf_voxelXform   : register(t13, space0);  // voxel AABB (float3 min + float3 max as uint)

RWByteAddressBuffer buf_output       : register(u0, space0);   // R11G11B10 shaded irradiance per strand CP

// ============================================================
// Textures
// ============================================================

TextureCubeArray              tex_envCubeArr   : register(t0, space0);   // env cube array
Texture2DArray                tex_blueNoise32  : register(t1, space0);   // 32x32 blue noise
Texture3D<uint4>              tex_dsVolume     : register(t2, space0);   // voxel cluster volume
Texture2D                     tex_shadowDepth  : register(t3, space0);   // shadow depth
TextureCube                   tex_envCube      : register(t4, space0);   // env cube
Texture2DArray                tex_shadowArr0   : register(t5, space0);   // shadow array (non-PCF)
Texture2DArray                tex_shadowArrPCF : register(t6, space0);   // shadow array (comparison)
Texture1DArray                tex_lut1D        : register(t7, space0);   // 1D LUT
Texture2D                     tex_dsLutNTT     : register(t8, space0);   // NTT azimuthal LUT
Texture3D                     tex_dsLut3D      : register(t9, space0);   // 3D dual-scattering LUT

// ============================================================
// Samplers
// ============================================================

SamplerState            smp_linear    : register(s0, space32);
SamplerState            smp_bilinear  : register(s1, space32);
SamplerState            smp_trilinear : register(s2, space32);
SamplerComparisonState  smp_shadow    : register(s3, space32);

// ============================================================
// MAIN COMPUTE ENTRY POINT
// ============================================================

[numthreads(64, 1, 1)]
void CS_HairShade(uint3 DTid : SV_DispatchThreadID)
{
    // --------------------------------------------------------
    // 3.1  Dispatch guard
    // --------------------------------------------------------
    uint strandCount = asuint(cb_dispatch[0].x);
    if (DTid.x >= strandCount)
        return;

    // --------------------------------------------------------
    // 3.2  Active bitmask check (1 bit per strand)
    // --------------------------------------------------------
    uint maskWord = buf_activeMask.Load(((DTid.x >> 5u)) * 4u);
    bool isActive = (maskWord & (1u << (DTid.x & 31u))) != 0u;
    if (!isActive)
        return;

    // --------------------------------------------------------
    // 3.3  Strand metadata: top 4 bits = flags, low 28 = cpBase
    // --------------------------------------------------------
    uint strandData  = buf_strandIndex.Load(DTid.x * 4u);
    uint strandFlags = strandData >> 28u;
    uint cpBase      = strandData & 0x0FFFFFFFu;

    // --------------------------------------------------------
    // 3.4  Current control-point position (float3, stride=4 uint)
    // --------------------------------------------------------
    uint cpOff = cpBase * 4u;
    float3 posC = asfloat(uint3(
        buf_strandCP.Load((cpOff + 0u) * 4u),
        buf_strandCP.Load((cpOff + 1u) * 4u),
        buf_strandCP.Load((cpOff + 2u) * 4u)));
    float posX = posC.x, posY = posC.y, posZ = posC.z;

    // Next CP (forward tangent)
    uint cpOff1 = (cpBase + 1u) * 4u;
    float3 posN = asfloat(uint3(
        buf_strandCP.Load((cpOff1 + 0u) * 4u),
        buf_strandCP.Load((cpOff1 + 1u) * 4u),
        buf_strandCP.Load((cpOff1 + 2u) * 4u)));

    // --------------------------------------------------------
    // 3.5  Tangent calculation (bi-directional, conditional at root/tip)
    // --------------------------------------------------------
    float3 tangPrev = float3(0, 0, 0);
    if ((strandFlags & 1u) == 0u)   // not root: read previous CP
    {
        uint cpOffP = (cpBase - 1u) * 4u;
        float3 posPrev = asfloat(uint3(
            buf_strandCP.Load((cpOffP + 0u) * 4u),
            buf_strandCP.Load((cpOffP + 1u) * 4u),
            buf_strandCP.Load((cpOffP + 2u) * 4u)));
        float3 dP  = posC - posPrev;
        tangPrev   = dP * rsqrt(dot(dP, dP));
    }

    float3 tangNext = float3(0, 0, 0);
    if ((strandFlags & 2u) == 0u)   // not tip: use forward CP
    {
        float3 dN  = posN - posC;
        tangNext   = dN * rsqrt(dot(dN, dN));
    }

    float3 tangSum = tangPrev + tangNext;
    float3 T = tangSum * rsqrt(dot(tangSum, tangSum));  // hair tangent (unit)

    // Per-strand scalar attributes packed in 4th uint of cpBase
    //   bits[31:24] = strandU1 (normalised along-strand param 1)
    //   bits[23:16] = strandU2 (normalised along-strand param 2)
    uint   cpExtra  = buf_strandCP.Load((cpOff + 3u) * 4u);
    float  strandU1 = float(cpExtra >> 24u)          * (1.0f / 255.0f);
    float  strandU2 = float((cpExtra >> 16u) & 255u)  * (1.0f / 255.0f);

    // --------------------------------------------------------
    // 5.1  View vector
    // --------------------------------------------------------
    float3 camPos = float3(cb0[7].w, cb0[8].w, cb0[9].w);
    float3 V      = normalize(camPos - posC);

    // --------------------------------------------------------
    // 5.2  Tangent-frame: bitangent & normal
    // --------------------------------------------------------
    float3 Tcross = cross(T, camPos - posC);
    Tcross = Tcross * rsqrt(dot(Tcross, Tcross));
    float3 N = cross(Tcross, T);

    // --------------------------------------------------------
    // 5.3  Screen-space pixel coordinate for blue-noise lookup
    // --------------------------------------------------------
    float clipX    = mad(posZ, cb0[2].w, mad(posY, cb0[1].w, cb0[0].w * posX)) + cb0[3].w;
    float screenU  = (mad(posZ, cb0[2].x, mad(posY, cb0[1].x, cb0[0].x * posX)) + cb0[3].x) / clipX;
    float screenV  = (mad(posZ, cb0[2].y, mad(posY, cb0[1].y, cb0[0].y * posX)) + cb0[3].y) / clipX;

    uint pu = (uint)(cb0[23].x * (screenU * 0.5f + 0.5f)) & 31u;
    uint pv = (uint)(cb0[23].y * (0.5f - screenV * 0.5f)) & 31u;

    // --------------------------------------------------------
    // 5.4  Blue-noise rotation angle for PCF jitter
    // --------------------------------------------------------
    float bnAngle = tex_blueNoise32.Load(int4(pu, pv, 0, 0)).x * 6.28318548f;
    float bnSin   = sin(bnAngle);
    float bnCos   = cos(bnAngle);

    // --------------------------------------------------------
    // 4.1  Base / Specular sRGB -> linear
    // --------------------------------------------------------
    float3 baseColorLinear = SrgbToLinear3(cb6[0].xyz);
    float3 specColorLinear = SrgbToLinear3(cb6[1].xyz);

    // --------------------------------------------------------
    // 4.2  First-stage smoothstep ramp (base -> specular)
    // --------------------------------------------------------
    float rangeA   = max(cb6[3].x, cb6[3].y) - cb6[3].x;
    float uRampA   = (rangeA > 1e-6f) ? saturate((strandU1 - cb6[3].x) / rangeA) : 0.0f;
    float uRampAsmooth = uRampA * uRampA * (3.0f - 2.0f * uRampA);
    float3 colorMid = lerp(baseColorLinear, specColorLinear, uRampAsmooth);

    // --------------------------------------------------------
    // 4.3  Second-stage smoothstep ramp (mid -> third color)
    // --------------------------------------------------------
    float3 color2Linear  = SrgbToLinear3(cb6[2].xyz);
    float  rangeB        = max(cb6[3].z, cb6[3].w) - cb6[3].z;
    float  uRampB        = (rangeB > 1e-6f) ? saturate((strandU1 - cb6[3].z) / rangeB) : 0.0f;
    float  uRampBsmooth  = uRampB * uRampB * (3.0f - 2.0f * uRampB);
    float  stage2Active  = (uRampAsmooth >= 1.0f) ? 1.0f : 0.0f;
    float  b2 = uRampBsmooth * uRampBsmooth;
    float3 finalBaseColor = colorMid + (b2 * (color2Linear - colorMid)
                            * (3.0f - 2.0f * uRampBsmooth)) * stage2Active;

    // --------------------------------------------------------
    // 4.4  HSV extraction from finalBaseColor, then hue/sat/val modulation
    // --------------------------------------------------------
    float maxC   = max(max(finalBaseColor.r, finalBaseColor.g), finalBaseColor.b);
    float minC   = min(min(finalBaseColor.r, finalBaseColor.g), finalBaseColor.b);
    float delta  = maxC - minC;
    float deltaInv = (delta > 1e-6f) ? (1.0f / delta) : 0.0f;

    float hue;
    if (finalBaseColor.r == maxC)
        hue = (finalBaseColor.g - finalBaseColor.b) * deltaInv;
    else if (finalBaseColor.g == maxC)
        hue = ((finalBaseColor.b - finalBaseColor.r) * deltaInv) + 2.0f;
    else
        hue = ((finalBaseColor.r - finalBaseColor.g) * deltaInv) + 4.0f;

    float hueN     = frac(hue * 0.16666667f);

    // Hue shift, saturation clamp, value scale (cb6[4/5])
    float hueShift = frac(frac(cb6[4].y + strandU1) * 2.0f * cb6[4].x - 0.5f + frac(hueN));
    float satVal   = (maxC > 1e-6f) ? (delta / maxC) : 0.0f;
    float satClamp = saturate(min(0.999f, cb6[4].z) * (frac(cb6[4].w + strandU1) - satVal));
    float valScale = saturate(min(0.999f, cb6[5].x) * (frac(cb6[5].y + strandU1) - maxC));

    // --------------------------------------------------------
    // 4.5  Reconstruct hairColor via HsvToRgb + absorption coefficients
    // --------------------------------------------------------
    float3 hairColor = HsvToRgb(hueShift, satClamp + satVal, valScale + maxC);

    // Absorption: sigma = log2(hairColor) * 0.11771371
    float sigmaR = log2(max(hairColor.r, 1e-6f)) * 0.11771371f;
    float sigmaG = log2(max(hairColor.g, 1e-6f)) * 0.11771371f;
    float sigmaB = log2(max(hairColor.b, 1e-6f)) * 0.11771371f;
    float sigRsq = sigmaR * sigmaR;
    float sigGsq = sigmaG * sigmaG;
    float sigBsq = sigmaB * sigmaB;

    // Marschner lobe widths (sqrt of variance stored in cb3)
    float betaR   = sqrt(max(cb3[1].z, 1e-6f));
    float betaTT  = sqrt(max(cb3[1].y, 1e-6f));
    float betaTRT = sqrt(max(cb3[1].w, 1e-6f));

    // --------------------------------------------------------
    // 6.1  Directional light setup
    // --------------------------------------------------------
    float3 dirLightColor = cb1[5].xyz;
    bool   dirLightEnabled = ((asuint(cb1[4].w) & 255u) & asuint(cb0[32].z)) != 0u;

    float dirLightR = 0.0f, dirLightG = 0.0f, dirLightB = 0.0f;

    if (dirLightEnabled)
    {
        // --------------------------------------------------------
        // 6.2  Directional light direction + windowed distance attenuation
        // --------------------------------------------------------
        float dlX  = cb1[4].x * 1e9f;
        float dlY  = cb1[4].y * 1e9f;
        float dlZ  = cb1[4].z * 1e9f;
        float dlLen = length(float3(dlX, dlY, dlZ));
        float3 L   = float3(dlX, dlY, dlZ) / max(dlLen, 1e-6f);

        precise float dlDist2 = dlLen * dlLen;
        precise float dlDist4 = dlDist2 * dlDist2;
        float atten0     = 1.0f - dlDist4 * 6.25e-38f;
        float atten      = saturate(atten0);
        float distPlusOne= (atten * atten + 1.0f)
                         + float(int(uint(dlLen < 0.0f) - uint(dlLen > 0.0f)));
        float distTerm   = saturate(distPlusOne);
        float windowed   = (distTerm * distTerm) * saturate(distTerm) * (3.0f - distTerm * 2.0f);

        if (windowed > 0.0f)
        {
            // --------------------------------------------------------
            // 6.3  Shadow cascade 0 UV transform
            // --------------------------------------------------------
            float3 hairPosCm  = posC * 100.0f;
            float  ps         = 0.01f;   // position scale cm->shadow units

            float3 shadowPos0 = float3(
                mad(hairPosCm.z * ps, cb1[14].x, mad(hairPosCm.y * ps, cb1[13].x, hairPosCm.x * ps * cb1[12].x)) + cb1[15].x,
                mad(hairPosCm.z * ps, cb1[14].y, mad(hairPosCm.y * ps, cb1[13].y, hairPosCm.x * ps * cb1[12].y)) + cb1[15].y,
                mad(hairPosCm.z * ps, cb1[14].z, mad(hairPosCm.y * ps, cb1[13].z, hairPosCm.x * ps * cb1[12].z)) + cb1[15].z);

            uint   cascadeIdx  = asuint(cb1[16].y);
            float  shadowDepth = cb1[16].w + shadowPos0.z;
            float2 shadowUV    = shadowPos0.xy;
            float2 shadowUVsc  = cb1[18].z * shadowPos0.xy + cb1[18].xy;

            // --------------------------------------------------------
            // 6.4 + 6.5  PCF 5x5 kernel with cascade fallback
            // --------------------------------------------------------
            float shadowAccum  = 0.0f;
            float shadowWeight = 0.0f;

            [unroll]
            for (uint py = 0u; py < 5u; py++)
            {
                [unroll]
                for (uint px = 0u; px < 5u; px++)
                {
                    float2 offset     = float2((float)px - 2.0f, (float)py - 2.0f) / cb1[3].xy;
                    float2 sampleUV   = offset + shadowUV;
                    bool   outOfBounds= (max(abs(sampleUV.x - 0.5f), abs(sampleUV.y - 0.5f)) > 0.5f);

                    float2 finalUV    = sampleUV;
                    uint   finalCasc  = cascadeIdx;
                    float  finalDepth = shadowDepth;

                    if (outOfBounds && cascadeIdx < 3u)
                    {
                        finalCasc++;
                        finalUV    = offset + shadowUVsc;
                        finalDepth = cb1[21].x * cb1[16].w + shadowPos0.z;
                    }

                    float s = 1.0f - tex_shadowArrPCF.SampleCmpLevelZero(
                        smp_shadow,
                        float3(finalUV, (float)finalCasc),
                        finalDepth);

                    shadowAccum  += s;
                    shadowWeight += 1.0f;
                }
            }

            float shadowFactor = shadowAccum / shadowWeight;

            if (shadowFactor > 0.0f)
            {
                // --------------------------------------------------------
                // 7.1  Longitudinal angles
                // --------------------------------------------------------
                float cosThI = dot(T, L);
                float cosThR = dot(T, V);

                float3 Li_perp = L - cosThI * T;
                float3 Lr_perp = V - cosThR * T;
                float  cosPhiD = dot(Li_perp, Lr_perp)
                               * rsqrt(dot(Li_perp, Li_perp) * dot(Lr_perp, Lr_perp) + 1e-4f);
                float  phiD       = cosPhiD * 0.5f + 0.5f;
                float  cosHalfPhi = sqrt(saturate(phiD));

                float sinThI_sq   = 1.0f - cosThI * cosThI;
                float sinThI_sqrt = sqrt(max(sinThI_sq, 0.0f));

                // --------------------------------------------------------
                // 7.2  NTT LUT: azimuthal distribution for TT lobe
                // --------------------------------------------------------
                float2 nttUV    = float2(0.5f - cosThI * 0.5f, betaTT);
                float4 nttSamp  = tex_dsLutNTT.SampleLevel(smp_linear, nttUV, 0.0f);
                float  nttA     = nttSamp.x;
                float  nttB     = nttSamp.y;

                // --------------------------------------------------------
                // 7.3  3D LUT: per-channel Fresnel weights
                // --------------------------------------------------------
                float cosThAbs = saturate(abs(cosThI));
                float4 lut3dR  = tex_dsLut3D.SampleLevel(smp_linear,
                    float3(cosThAbs, saturate(betaR),   saturate(sqrt(max(sigmaR, 0.0f)))), 0.0f);
                float4 lut3dG  = tex_dsLut3D.SampleLevel(smp_linear,
                    float3(cosThAbs, saturate(betaTT),  saturate(sqrt(max(sigmaG, 0.0f)))), 0.0f);
                float4 lut3dB  = tex_dsLut3D.SampleLevel(smp_linear,
                    float3(cosThAbs, saturate(betaTRT), saturate(sqrt(max(sigmaB, 0.0f)))), 0.0f);

                // Clamp Fresnel to [0, 0.99]
                float3 fresnel0 = float3(min(lut3dR.x, 0.99f), min(lut3dG.x, 0.99f), min(lut3dB.x, 0.99f));
                float3 fresnel1 = float3(min(lut3dR.y, 0.99f), min(lut3dG.y, 0.99f), min(lut3dB.y, 0.99f));
                float3 one_f0   = 1.0f - fresnel0;
                float3 one_f1   = 1.0f - fresnel1;

                // --------------------------------------------------------
                // 7.4  Longitudinal mean shifts (alpha tilt from cb3[1].x)
                // --------------------------------------------------------
                float alpha    = cb3[1].x;
                float thetaI   = asin(clamp(cosThI, -1.0f, 1.0f));
                float thetaR   = asin(clamp(cosThR, -1.0f, 1.0f));
                float thetaH   = (thetaI + thetaR) * 0.5f;

                // --------------------------------------------------------
                // 7.5  Longitudinal Gaussian M for each lobe
                // --------------------------------------------------------
                float M_R   = LongitudinalGaussian(thetaH - alpha,          betaR   * betaR);
                float M_TT  = LongitudinalGaussian(thetaH - alpha * 0.5f,  betaTT  * betaTT  * 0.5f);
                float M_TRT = LongitudinalGaussian(thetaH + alpha * 1.5f,  betaTRT * betaTRT * 2.0f);

                // --------------------------------------------------------
                // 7.6  Per-channel absorption terms (exp2 form)
                // --------------------------------------------------------
                // A(p=0): Fresnel reflection (R lobe)
                // A(p=1): (1-F)^2 * T   (TT lobe), T = exp2(sigma^2 * -1.44... * path)
                // A(p=2): (1-F)^2 * F * T^2 (TRT lobe)
                float absR = exp2(sigRsq * (-1.44269502f));
                float absG = exp2(sigGsq * (-1.44269502f));
                float absB = exp2(sigBsq * (-1.44269502f));

                // TT path absorption (double pass through fibre)
                float3 T_TT  = float3(absR * absR, absG * absG, absB * absB);
                // TRT path absorption (quadruple effective pass)
                float3 T_TRT = float3(T_TT.r * T_TT.r, T_TT.g * T_TT.g, T_TT.b * T_TT.b);

                // Azimuthal weight from LUT3D channels (.z = R azimuth, .w = TRT azimuth)
                float3 az_R   = float3(lut3dR.z, lut3dG.z, lut3dB.z);
                float3 az_TRT = float3(lut3dR.w, lut3dG.w, lut3dB.w);

                // NTT azimuthal for TT (combine nttA with single-pass absorption)
                float3 az_TT  = float3(nttA * absR, nttA * absG, nttA * absB);

                // --------------------------------------------------------
                // 7.7  [P1] Per-channel Marschner BSDF accumulation
                //       result_ch = M_R  * F0_ch   * az_R_ch
                //                 + M_TT * (1-F0)^2 * T_TT_ch * az_TT_ch
                //                 + M_TRT* (1-F0)^2 * F1_ch * T_TRT_ch * az_TRT_ch
                //       scaled by shadowFactor * windowed * sinThI_sqrt
                // --------------------------------------------------------
                float scale = shadowFactor * windowed * sinThI_sqrt;

                float3 bsdf_R   = M_R   * fresnel0 * az_R;
                float3 bsdf_TT  = M_TT  * (one_f0 * one_f0) * T_TT  * az_TT;
                float3 bsdf_TRT = M_TRT * (one_f0 * one_f0) * fresnel1 * T_TRT * az_TRT;

                float3 bsdfTotal = (bsdf_R + bsdf_TT + bsdf_TRT) * scale;

                // --------------------------------------------------------
                // 7.8  Multiply by directional light color
                // --------------------------------------------------------
                dirLightR = bsdfTotal.r * dirLightColor.x;
                dirLightG = bsdfTotal.g * dirLightColor.y;
                dirLightB = bsdfTotal.b * dirLightColor.z;
            }
        }
    }

    // --------------------------------------------------------
    // 8.1 ~ 8.4  Point light path (voxel-culled, framework only)
    // --------------------------------------------------------
    float pointLightR = 0.0f, pointLightG = 0.0f, pointLightB = 0.0f;

    // 8.1  Look up voxel cell for current hair position
    // tex_dsVolume is a uint4 3D texture encoding cluster cell indices.
    // (Full traversal omitted – see hair_shade.hlsl lines 1226-1252)

    // 8.2  Read per-cell light bitmask
    // uint cellWordIdx = ...; // derived from volume lookup
    // uint lightMask = buf_clusterMask.Load(cellWordIdx * 4u);
    // (bit-scan loop over active lights omitted)

    // TODO P2: full point-light BSDF loop (see hair_shade.hlsl lines 1226-3600)

    // 8.4  pointLight contribution initialised to zero (framework placeholder)
    //      pointLightR/G/B remain 0.0 until P2 is implemented.

    // --------------------------------------------------------
    // 9.1  IBL / Environment light (placeholder)
    // --------------------------------------------------------
    float envR = 0.0f, envG = 0.0f, envB = 0.0f;
    // TODO P3: IBL from tex_envCube (see hair_shade.hlsl lines 3600-4140)

    // --------------------------------------------------------
    // 10.1  Combine all contributions
    // --------------------------------------------------------
    float outR = dirLightR + pointLightR + envR;
    float outG = dirLightG + pointLightG + envG;
    float outB = dirLightB + pointLightB + envB;

    // --------------------------------------------------------
    // 10.2  Pack as R11G11B10
    // --------------------------------------------------------
    uint packed = PackR11G11B10(outR, outG, outB);

    // --------------------------------------------------------
    // 10.3  Write to cpBase+1 (default per-CP output slot)
    // --------------------------------------------------------
    buf_output.Store((cpBase + 1u) * 4u, packed);

    // --------------------------------------------------------
    // 10.4  Double-write to cpBase when root or prev-CP inactive
    // --------------------------------------------------------
    uint prevID      = (DTid.x == 0u) ? 0u : (DTid.x - 1u);
    uint prevMaskWrd = buf_activeMask.Load((prevID >> 5u) * 4u);
    bool prevInactive= (prevMaskWrd & (1u << (prevID & 31u))) == 0u;

    if ((strandFlags == 1u) || prevInactive)
    {
        buf_output.Store(cpBase * 4u, packed);
    }
}
