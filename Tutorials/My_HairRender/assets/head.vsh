cbuffer Constants
{
    float4x4 g_WorldViewProj;
    float4   g_Offset;
};

struct HeadPosition
{
    float3 Pos;
};
StructuredBuffer<HeadPosition> PositionArray;

struct HeadTexcoord
{
    float2 UV;
};
StructuredBuffer<HeadTexcoord> TexcoordArray;

struct HeadNormalTangent
{
    uint PackedNormal;   // signed byte4: x,y,z,w
    uint PackedTangent;  // signed byte4: x,y,z,w
};
StructuredBuffer<HeadNormalTangent> NormalTangentArray;

float3 UnpackSByte4(uint packed)
{
    int x = (int)(packed <<  24) >> 24;
    int y = (int)(packed <<  16) >> 24;
    int z = (int)(packed <<   8) >> 24;
    return clamp(float3(x, y, z) / 127.0, -1.0, 1.0);
}

struct VSInput
{
    uint VertexIdx : SV_VertexID;
};

struct PSInput
{
    float4 Pos    : SV_POSITION;
    float3 Normal : NORMAL0;
    float2 UV     : TEXCOORD0;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    uint idx = VSIn.VertexIdx;

    float3 P = PositionArray[idx].Pos;
    float3 VPos;
    // VPos.x = (P.x + g_Offset.x) * 100.0;
    // VPos.y = (P.y + g_Offset.y) * 100.0;
    // VPos.z = (P.z + g_Offset.z) * 100.0;
    VPos.x = (P.x) * 100.0;
    VPos.y = (P.y) * 100.0;
    VPos.z = (P.z) * 100.0;
    float3 VNormal = UnpackSByte4(NormalTangentArray[idx].PackedNormal);
    float2 VUV = TexcoordArray[idx].UV;

    PSIn.Pos    = mul(float4(VPos, 1.0), g_WorldViewProj);
    PSIn.Normal = VNormal;
    PSIn.UV     = VUV;
}
