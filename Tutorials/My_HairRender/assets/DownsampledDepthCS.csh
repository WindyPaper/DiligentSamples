//Down sampled Depth map to 1/16

#include "CommonCS.csh"

Texture2D InputSrcDepthMap;
// SamplerState InputSrcDepthMap_sampler;

RWTexture2D<float4> OutputMipDepthMap;

groupshared uint MinValueIn16X16;

[numthreads(16, 16, 1)]
void CSMain(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID, uint group_idx : SV_GroupIndex)
{
    MinValueIn16X16 = 0u;    
    GroupMemoryBarrier();

	//Get texture size
	uint src_tex_x = id.x;
	uint src_tex_y = id.y;

    uint sample_src_x = min(src_tex_x, uint(ScreenSize.x) - 1);
    uint sample_src_y = min(src_tex_y, uint(ScreenSize.y) - 1);
    
	uint src_depth = asuint(InputSrcDepthMap.Load(int3(sample_src_x, sample_src_y, 0)).r);
    uint src_value;
    InterlockedMax(MinValueIn16X16, src_depth, src_value);
    GroupMemoryBarrier();
	
    if(group_idx == 0u)
    {
        OutputMipDepthMap[group_id.xy] = asfloat(MinValueIn16X16);
    }
}
