cbuffer Constants
{
    float4x4 g_WorldViewProj;
};

// Vertex shader takes two inputs: vertex position and color.
// By convention, Diligent Engine expects vertex shader inputs to be 
// labeled 'ATTRIBn', where n is the attribute number.
struct VSInput
{
    //float4 Pos   : ATTRIB0;    
    uint VertexIdx : SV_VertexID;
};

StructuredBuffer<uint> VertexIdxArray;

struct HairVertexData
{
    float4 Pos;
};
StructuredBuffer<HairVertexData> VertexDataArray;

struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float4 Color : COLOR0;
};

// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be indentical.
void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    // PSIn.Pos   = mul( float4(VSIn.Pos,1.0), g_WorldViewProj);
    // PSIn.Color = VSIn.Color;

    uint VertexIdx = VSIn.VertexIdx;
    uint CurrVertexIdx = (VertexIdxArray[VertexIdx >> 1u] & 268435455u) + (VertexIdx & 1u);
    float3 VPos = VertexDataArray[CurrVertexIdx].Pos.xyz;
    //float3 NewVPos = (VPos - float3(299.5f, -6.7f, -6.2f)) * 100.0f;

    PSIn.Pos = mul( float4(VPos,1.0), g_WorldViewProj);
    PSIn.Color = float4(1.0f, 0.0f, 0.0f, 1.0f);
}
