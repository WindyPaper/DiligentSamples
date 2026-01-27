//Calculate hair vertex shading data

#include "CommonCS.csh"

cbuffer LightData
{
    float4 DirectionLightDir;
    float4 DirectionLightColor;
};

struct HairVertexData
{
    float3 Pos;
    int Misc;
};
StructuredBuffer<HairVertexData> VerticesDatas;
StructuredBuffer<uint> IdxData;

// ByteAddressBuffer LineSizeBuffer;

StructuredBuffer<uint> LineVisibilityBuffer;

RWStructuredBuffer<uint> OutHairVertexShadeData;

// groupshared uint GroupNumAccum;

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID, uint group_thread_idx : SV_GroupIndex)
{
    if ((LineVisibilityBuffer[id.x >> 5u] & (1u << (id.x & 31u))) != 0u)
    {
        uint LineIdx0 = id.x;
        uint VertexType = IdxData[LineIdx0] >> 28u;
        uint VertexIdx0 = IdxData[LineIdx0] & 0x0FFFFFFF;

        HairVertexData V0 = VerticesDatas[VertexIdx0];
        if(!isnan(V0.Pos.x))
        {
            HairVertexData V1 = VerticesDatas[VertexIdx0 + 1];

            float3 tangent = normalize(V1.Pos - V0.Pos);

            

        

            OutHairVertexShadeData[VertexIdx0 + 1] = PackR11G11B10F(float3(1.0f, 1.0f, 1.0f));

            if(VertexType == 1)
            {
                OutHairVertexShadeData[VertexIdx0] = PackR11G11B10F(float3(1.0f, 1.0f, 1.0f));
            }
        }
    }
}
