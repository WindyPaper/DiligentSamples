//Get flat voxel work queue for line rendering

#include "CommonCS.csh"

#define VOXEL_SLICE_NUM 24

ByteAddressBuffer LineSizeBuffer;

RWStructuredBuffer<uint> OutWorkQueueBuffer;

RWByteAddressBuffer OutWorkQueueCountBuffer;

groupshared uint GroupNumAccum;

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID, uint group_thread_idx : SV_GroupIndex)
{
    if(group_thread_idx == 0)
    {
        GroupNumAccum = 0;
    }
    GroupMemoryBarrier();

    uint WorkQueueVal = 4294967295u;
    uint LocalOffsetNum = 0;
    if(id.x < (uint)DownSampleDepthSize.x && id.y < (uint)DownSampleDepthSize.y)
    {        
        // uint FstOffsetValIdx = 0;
        for(uint slice_idx = 0; slice_idx < VOXEL_SLICE_NUM; ++slice_idx)
        {
            uint OffsetIdx = (id.y * DownSampleDepthSize.x + id.x) * VOXEL_SLICE_NUM + slice_idx;
            uint CurrSizeNum = LineSizeBuffer.Load(OffsetIdx * 4);

            if(CurrSizeNum != 0)
            {
                InterlockedAdd(GroupNumAccum, 1, LocalOffsetNum);
                // FstOffsetValIdx = slice_idx;
                WorkQueueVal = ((id.y << 12u) | id.x) | (slice_idx << 24u);                
                break;
            }                  
        }
    }

    GroupMemoryBarrierWithGroupSync();
    
    if(group_thread_idx == 0)
    {
        //InterlockedAdd(OutWorkQueueCountBuffer[0], GroupNumAccum, GroupNumAccum);
        OutWorkQueueCountBuffer.InterlockedAdd(0, GroupNumAccum, GroupNumAccum);
    }

    GroupMemoryBarrierWithGroupSync();

    if(WorkQueueVal != 4294967295u)
    {
        OutWorkQueueBuffer[GroupNumAccum + LocalOffsetNum] = WorkQueueVal;
    }
    //}



}
