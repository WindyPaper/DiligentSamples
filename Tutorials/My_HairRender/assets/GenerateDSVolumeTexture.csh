// Generate hair deep-shadow density volume (DSVolumeTexture).
// Faithful port of DSDepthGenerate.dxil (rasterDepthDSVolume):
//   - One thread per voxel (4x4x4 groups).
//   - Reconstruct voxel world position by lerping the hair AABB.
//   - Project to screen via ViewProj, sample scene depth.
//   - Reconstruct the visible surface world position via InvViewProj.
//   - If the voxel is occluded (behind surface) and lies close to the surface
//     along the light direction, mark it by OR-ing the top 8 bits.
//
// Runs before LineVertexShading.csh.

#include "CommonCS.csh"

// SceneInfo (matches DXIL cbuffer b0 rows used):
//   ViewProj      : world -> clip                (DXIL rows 0..3)
//   LightDir      : light direction (world)      (DXIL row 6)
//   InvViewProj   : clip -> world                (DXIL rows 14..17)
//   DepthSize     : depth texture size in texels (DXIL row 23)
cbuffer DSVolumeSceneInfo
{
    float4x4 DSV_ViewProj;     // stored transposed; consume via mul(pos, M)
    float4x4 DSV_InvViewProj;  // stored transposed; consume via mul(pos, M)
    float4   DSV_LightDir;   // xyz used
    float4   DSV_DepthSize;  // xy = depth texel size
    float4   DSV_Tolerance;  // x = distance tolerance along light dir
};

// DSVolumeInfo (matches DXIL SSBO _14, uvec3[] laid out as float3/uint3 rows):
//   [0] mMinAABB         (float bits) xyz
//   [1] mMaxAABB         (float bits) xyz
//   [2] mResolution      (uint)       xyz
//   [3] mClearResolution (uint)       xyz   (unused here)
//   [4] mScale           (float bits) xyz   -> DXIL uses to map thread -> voxel
//   [5] mInvLength       (float bits) xyz   (unused here)
//   [6] mInvResolution   (float bits = 1/res) xyz (unused by main entry)
// NOTE: DXIL reads _60 = _m0[4] (mScale, value {1,1,1}) and maps
//       voxel coord = uint(mScale * GlobalInvocationID). With scale==1 this
//       equals gid. Keep the same row order so index [4] is mScale.
cbuffer DSVolumeInfo
{
    float4 mMinAABB;          // xyz
    float4 mMaxAABB;          // xyz
    uint4  mResolution;       // xyz
    uint4  mClearResolution;  // xyz (unused)
    float4 mScale;            // xyz -> thread-to-voxel scale (DXIL _m0[4])
    float4 mInvLength;        // xyz (unused)
    float4 mInvResolution;    // xyz = 1 / resolution (unused by main)
};

Texture2D<float>  SceneDepth;         // scene/hair depth (t0)
RWTexture3D<uint> DSVolumeTexture;    // r32ui deep-shadow volume (u0)

// ---------------------------------------------------------------------------
// GenerateDSVolumeTextureFromHair
// Faithful port of GenerateDSVolumeTextureFromHair.hlsl (dxil-spirv dump).
//   - One thread per hair strand (idx < HairStrandCount).
//   - Reads the strand's two consecutive vertices from the vertex buffer.
//   - Marches the segment in steps of DSInfo_VoxelWorldSize and trilinearly
//     splats a density value into the r32ui volume via InterlockedAdd.
//
// Runs BEFORE the CSMain (rasterDepthDSVolume) entry in this file.
// ---------------------------------------------------------------------------

// Strand count cbuffer (DXIL _28._m0[0].x, read as uint bits).
cbuffer HairStrandCountInfo
{
    uint4 HairStrandCount;   // x = number of strands
};

// DSInfo cbuffer (DXIL _33). Only the members used by this entry are named.
//   _33._m0[0].x = DSInfo_VoxelWorldSize
//   _33._m0[1].x = DSInfo_VolumeTracingOffsetScale (used by shading)
//   _33._m0[1].w = DSInfo_VolumeTracingIBLDelta (used by shading)
//   _33._m0[2].x = DSInfo_VolumeTracingDelta (used by shading)
//   _33._m0[2].y = DSInfo_VolumePageResolution
//   _33._m0[3].x = DSInfo_RasterDepthThreshold
//   _33._m0[3].y = Material.hm_backscatterScale (used by shading)
cbuffer DSInfo
{
    float4 DSInfo_Row0;   // x = VoxelWorldSize
    float4 DSInfo_Row1;   // x = VolumeTracingOffsetScale, w = VolumeTracingIBLDelta
    float4 DSInfo_Row2;   // x = VolumeTracingDelta, y = VolumePageResolution
    float4 DSInfo_Row3;   // x = RasterDepthThreshold, y = BackscatterScale
};

// Strand -> first-vertex index list (DXIL SSBO _9). idx = value & 0x0FFFFFFF.
StructuredBuffer<uint> HairStrandIdxData;

// Hair vertex buffer (DXIL SSBO _13, 4 uints per vertex).
//   Pos  : float3 (world, pre *100 unit-scale)
//   Misc : low 16 bits = packed width (u16 -> * 1/65535 / RasterDepthThreshold)
struct DSHairVertexData
{
    float3 Pos;
    int    Misc;
};
StructuredBuffer<DSHairVertexData> HairVerticesDatas;

[numthreads(64, 1, 1)]
void CSGenerateFromHair(uint3 gid : SV_DispatchThreadID)
{
    // One thread per strand (DXIL: if (strandCount > gid.x)).
    if (gid.x >= HairStrandCount.x)
        return;

    // Strand -> first vertex index.
    uint idx = HairStrandIdxData[gid.x] & 0x0FFFFFFFu;

    DSHairVertexData V0 = HairVerticesDatas[idx];
    DSHairVertexData V1 = HairVerticesDatas[idx + 1u];

    // World positions. NOTE: HairBBoxMin/Max (mMinAABB/mMaxAABB) are computed
    // from raw V.Pos (no unit scale), same as LineGetVisibilityCS. So use Pos
    // directly here; multiplying by 100 pushes every voxel outside the AABB and
    // makes the whole volume splat into one clamped corner (result all black).
    float3 p0 = V0.Pos;
    float3 p1 = V1.Pos;

    // DXIL early-out: skip if either endpoint is NaN.
    if (isnan(p0.x) || isnan(p1.x))
        return;

    float rasterThreshold = DSInfo_Row3.x;   // DSInfo_RasterDepthThreshold
    float voxelWorldSize  = DSInfo_Row0.x;    // DSInfo_VoxelWorldSize
    float pageResolution  = DSInfo_Row2.y;    // DSInfo_VolumePageResolution

    // Endpoint widths: low 16 bits -> normalized [0,1] then / RasterDepthThreshold.
    float w0 = max((float)((uint)V0.Misc & 0xFFFFu) * (1.0f / 65535.0f) / rasterThreshold, 0.0f);
    float w1 = max((float)((uint)V1.Misc & 0xFFFFu) * (1.0f / 65535.0f) / rasterThreshold, 0.0f);

    // Segment direction / length in unit-scale space.
    float3 seg    = p1 - p0;
    float  segLen = length(seg);
    float3 dir    = seg * rsqrt(dot(seg, seg));

    // Step vector = normalized dir * VoxelWorldSize.
    float3 step = dir * voxelWorldSize;

    // Number of march steps (DXIL: ceil(segLen / VoxelWorldSize), then max(.-1,1)).
    float stepCount    = ceil(segLen / voxelWorldSize);
    float stepCountM1  = max(stepCount - 1.0f, 1.0f);

    // Volume mapping data (DXIL SSBO _18).
    float3 minAABB      = mMinAABB.xyz;                 // _18._m0[0]
    uint3  resM1        = mResolution.xyz - 1u;         // _18._m0[2] - 1
    float3 scale        = mScale.xyz;                   // _18._m0[4]
    float3 invLength    = mInvLength.xyz;               // _18._m0[5]

    if (stepCount <= 0.0f)
        return;

    // Previous splatted voxel (to skip duplicates), init to invalid.
    uint3 prevVoxel = uint3(0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);

    for (float s = 0.0f; s < stepCount; s += 1.0f)
    {
        // Sample world position along the segment.
        float3 wp = step * s + p0;

        // Normalize into volume [0,1] via (wp - min) * invLength.
        float3 nrm = (wp - minAABB) * invLength;
        float3 tc  = saturate(nrm);

        // Flip Y/Z to match CSMain's voxel->world mapping (v = 1 - y/res,
        // w = 1 - z/res). Without this the density and the occlusion OR-mark
        // land in mirrored voxels and the shadow looks inverted.
        tc.y = 1.0f - tc.y;
        tc.z = 1.0f - tc.z;

        // Continuous voxel coordinate.
        float3 fc = tc * (float3)resM1;
        uint3  vc = (uint3)fc;

        // Skip if same voxel as previous step.
        bool changed = any(vc != prevVoxel);
        if (changed)
        {
            // Trilinear weights.
            float3 frac1 = fc - (float3)vc;
            float3 frac0 = 1.0f - frac1;

            uint3 vc1 = min(vc + 1u, resM1);

            // Density value (DXIL _264): lerp widths along the strand, scaled.
            float wLerp = (s / stepCountM1) * (w1 - w0) + w0;
            float density = wLerp * 0.005f * pageResolution / voxelWorldSize * 1000.0f;

            // 8-corner trilinear splat (r32ui, additive, 24-bit clamp).
            InterlockedAdd(DSVolumeTexture[uint3(vc.x,  vc.y,  vc.z )], (uint)round(density * frac0.x * frac0.y * frac0.z) & 0x00FFFFFFu);
            InterlockedAdd(DSVolumeTexture[uint3(vc1.x, vc.y,  vc.z )], (uint)round(density * frac1.x * frac0.y * frac0.z) & 0x00FFFFFFu);
            InterlockedAdd(DSVolumeTexture[uint3(vc.x,  vc1.y, vc.z )], (uint)round(density * frac0.x * frac1.y * frac0.z) & 0x00FFFFFFu);
            InterlockedAdd(DSVolumeTexture[uint3(vc1.x, vc1.y, vc.z )], (uint)round(density * frac1.x * frac1.y * frac0.z) & 0x00FFFFFFu);
            InterlockedAdd(DSVolumeTexture[uint3(vc.x,  vc.y,  vc1.z)], (uint)round(density * frac0.x * frac0.y * frac1.z) & 0x00FFFFFFu);
            InterlockedAdd(DSVolumeTexture[uint3(vc1.x, vc.y,  vc1.z)], (uint)round(density * frac1.x * frac0.y * frac1.z) & 0x00FFFFFFu);
            InterlockedAdd(DSVolumeTexture[uint3(vc.x,  vc1.y, vc1.z)], (uint)round(density * frac0.x * frac1.y * frac1.z) & 0x00FFFFFFu);
            InterlockedAdd(DSVolumeTexture[uint3(vc1.x, vc1.y, vc1.z)], (uint)round(density * frac1.x * frac1.y * frac1.z) & 0x00FFFFFFu);

            prevVoxel = vc;
        }
    }
}

[numthreads(4, 4, 4)]
void CSMain(uint3 gid : SV_DispatchThreadID)
{
    // Thread -> voxel coordinate. DXIL: coord = uint(mScale * gid) (mScale == {1,1,1}).
    uint3 coord = (uint3)(mScale.xyz * (float3)gid);
    if (any(coord >= mResolution.xyz))
        return;

    // Normalized position inside the volume (note Y and Z flipped, as in DXIL).
    float3 res_m1 = (float3)(mResolution.xyz - 1u);
    float u =        (float)coord.x / res_m1.x;
    float v = 1.0f - (float)coord.y / res_m1.y;
    float w = 1.0f - (float)coord.z / res_m1.z;
    float3 t = saturate(float3(u, v, w));

    // Voxel world position = lerp(min, max, t).
    float3 pos = lerp(mMinAABB.xyz, mMaxAABB.xyz, t);

    // Project to clip space.
    // ViewProj is stored transposed (same as LineGetVisibilityCS), so use the
    // row-vector convention mul(pos, M) to consume it. mul(M, pos) would apply
    // an extra transpose and give the wrong result.
    float4 clip = mul(float4(pos, 1.0f), DSV_ViewProj);
    float2 ndc  = clip.xy / clip.w;
    float2 uv   = float2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f);

    if (!(uv.x >= 0.0f && uv.x < 1.0f && uv.y >= 0.0f && uv.y < 1.0f))
        return;

    // Sample scene depth at the projected texel.
    int2 depthCoord = int2((int)(DSV_DepthSize.x * uv.x), (int)(DSV_DepthSize.y * uv.y));
    float sceneDepth = SceneDepth.Load(int3(depthCoord, 0));

    // Reconstruct the visible surface world position from depth via InvViewProj.
    // InvViewProj is stored transposed too -> row-vector convention mul(pos, M).
    // DXIL multiplies the reconstructed world position by 100 (unit scale) after
    // the perspective divide (see _224/_238/_248 = recon * 100.0).
    float4 reconH = mul(float4(ndc, sceneDepth, 1.0f), DSV_InvViewProj);
    float3 reconW = (reconH.xyz / reconH.w);

    // Depth range test: surface Z must fall within the volume's Z extent.
    if (!(reconW.z <= mMaxAABB.z && reconW.z >= mMinAABB.z))
        return;

    // Voxel clip-space depth (for the occlusion test below).
    float voxelClipZ = clip.z / clip.w;

    // Occluded voxel (behind surface) AND within tolerance along light dir.
    // DXIL compares against (DSInfo_RasterDepthThreshold * 100) -> _28._m0[0].w * 100.
    float distVoxel  = dot(pos,    DSV_LightDir.xyz);
    float distRecon  = dot(reconW, DSV_LightDir.xyz);
    bool  occluded   = sceneDepth > voxelClipZ;
    bool  nearShadow = abs(distVoxel - distRecon) < (DSV_Tolerance.x * 100.0f);

    if (occluded && nearShadow)
    {
        InterlockedOr(DSVolumeTexture[coord], 0xFF000000u);
    }
}

// Clear entry: zero the whole volume before accumulation.
[numthreads(4, 4, 4)]
void CSClear(uint3 gid : SV_DispatchThreadID)
{
    if (any(gid >= mResolution.xyz))
        return;
    DSVolumeTexture[gid] = 0u;
}
