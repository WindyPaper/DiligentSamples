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

    float3 P = PositionArray[idx].Pos + g_Offset.xyz;
    // Rotate 90 degrees around Y axis, then scale by 100
    float3 R;
    R.x =  P.z;
    R.y =  P.y;
    R.z = -P.x;
    float3 VPos;
    VPos.x = R.x * 100.0;
    VPos.y = R.y * 100.0;
    VPos.z = R.z * 100.0;
    float3 VNormal = UnpackSByte4(NormalTangentArray[idx].PackedNormal);
    // Apply the same 90-degree Y rotation to the normal
    VNormal = float3(VNormal.z, VNormal.y, -VNormal.x);
    float2 VUV = TexcoordArray[idx].UV;

    PSIn.Pos    = mul(float4(VPos, 1.0), g_WorldViewProj);
    PSIn.Normal = VNormal;
    PSIn.UV     = VUV;
}
