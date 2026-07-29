// Downsample the DSVolumeTexture mip chain (128 -> 64 -> ... -> 4).
// Faithful port of DepthVolumeDownsample.hlsl:
//   Per dst voxel, gather the 8 child voxels of the previous mip.
//   - top 8 bits  (coverage): average of children, min 1 if any covered.
//   - low 24 bits (density) : average of children, min 1 if any non-zero.
//   Pack: density24 | (coverage << 24).
//
// SrcVolume : previous mip (SRV, sampled at Lod 0 of the bound mip view)
// DstVolume : current  mip (UAV)

cbuffer DownsampleInfo
{
    uint4  DstMipLevel;   // x = destination mip level (used to shift resolution)
};

// DSVolumeInfo (only mResolution [row 2] is used here).
// Layout must match DSVolumeInfoCB / GenerateDSVolumeTexture.csh (shared buffer).
cbuffer DSVolumeInfo
{
    float4 mMinAABB;
    float4 mMaxAABB;
    uint4  mResolution;
    uint4  mClearResolution;
    float4 mScale;
    float4 mInvLength;
    float4 mInvResolution;
};

Texture3D<uint>   SrcVolume;
RWTexture3D<uint> DstVolume;

[numthreads(4, 4, 4)]
void CSMain(uint3 gid : SV_DispatchThreadID)
{
    uint mip = DstMipLevel.x & 31u;
    uint3 dstRes = mResolution.xyz >> mip;
    if (any(gid >= dstRes))
        return;

    uint3 c0 = gid << 1u;          // even child coords
    uint3 c1 = c0 | 1u;            // odd  child coords

    uint s0 = SrcVolume.Load(int4(c0.x, c0.y, c0.z, 0)).x;
    uint s1 = SrcVolume.Load(int4(c1.x, c0.y, c0.z, 0)).x;
    uint s2 = SrcVolume.Load(int4(c0.x, c1.y, c0.z, 0)).x;
    uint s3 = SrcVolume.Load(int4(c1.x, c1.y, c0.z, 0)).x;
    uint s4 = SrcVolume.Load(int4(c0.x, c0.y, c1.z, 0)).x;
    uint s5 = SrcVolume.Load(int4(c1.x, c0.y, c1.z, 0)).x;
    uint s6 = SrcVolume.Load(int4(c0.x, c1.y, c1.z, 0)).x;
    uint s7 = SrcVolume.Load(int4(c1.x, c1.y, c1.z, 0)).x;

    // Coverage (top byte).
    uint covSum = (s0 >> 24u) + (s1 >> 24u) + (s2 >> 24u) + (s3 >> 24u)
                + (s4 >> 24u) + (s5 >> 24u) + (s6 >> 24u) + (s7 >> 24u);
    float covFlag = (float)(covSum != 0u);
    float covAvg  = (float)covSum * 0.125f;
    float cov     = min(max(covAvg, covFlag), 255.0f);
    uint  covByte = (uint)cov;

    // Density (low 24 bits).
    uint denSum = (s0 & 0x00FFFFFFu) + (s1 & 0x00FFFFFFu) + (s2 & 0x00FFFFFFu) + (s3 & 0x00FFFFFFu)
                + (s4 & 0x00FFFFFFu) + (s5 & 0x00FFFFFFu) + (s6 & 0x00FFFFFFu) + (s7 & 0x00FFFFFFu);
    uint denAvg = (denSum != 0u) ? max(denSum >> 3u, 1u) : 0u;
    uint den24  = min(0x00FFFFFFu, denAvg);

    DstVolume[gid] = den24 | (covByte << 24u);
}
