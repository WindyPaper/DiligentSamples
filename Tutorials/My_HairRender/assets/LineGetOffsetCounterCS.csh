//Accumulate line size buffer to Offset buffer and counter

#include "CommonCS.csh"

#define VOXEL_SLICE_NUM 24

RWByteAddressBuffer LineAccumulateBuffer;

RWByteAddressBuffer OutLineOffsetBuffer;
RWByteAddressBuffer OutLineCounterBuffer;


[numthreads(8, 8, 4)]
void CSMain(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID, uint group_idx : SV_GroupIndex)
{
    uint curr_z = id.z;
    if(id.x < (uint)DownSampleDepthSize.x && id.y < (uint)DownSampleDepthSize.y)
    {
        for(; curr_z < VOXEL_SLICE_NUM; curr_z += 4)
        {
            uint LineAccuIdx = (id.y * DownSampleDepthSize.x + id.x) * VOXEL_SLICE_NUM + curr_z;
            uint LineSizeInVoxel = LineAccumulateBuffer.Load(LineAccuIdx);
            uint SrcAddVal = 0;

            if(LineSizeInVoxel != 0)
            {
                //InterlockedAdd(OutLineCounterBuffer[0], LineSizeInVoxel, SrcAddVal);
                OutLineCounterBuffer.InterlockedAdd(0, LineSizeInVoxel, SrcAddVal);

                OutLineOffsetBuffer.Store(LineAccuIdx, SrcAddVal);

                //reset
                LineAccumulateBuffer.Store(LineAccuIdx, 0);
            }            
        }
    }
}
