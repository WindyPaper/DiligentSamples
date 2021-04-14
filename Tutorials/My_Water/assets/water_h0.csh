//#include "structures.fxh"

cbuffer Constants
{
    GlobalConstants g_Constants;
};

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 64
#endif

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID)
{
    uint uiGlobalThreadIdx = Gid.x * uint(THREAD_GROUP_SIZE) + GTid.x;
    if (uiGlobalThreadIdx >= g_Constants.uiNumParticles)
        return;

    
}
