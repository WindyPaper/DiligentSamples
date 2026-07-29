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
