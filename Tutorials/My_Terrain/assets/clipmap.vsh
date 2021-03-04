cbuffer Constants
{
    float4x4 g_WorldViewProj;
};

cbuffer PerPatchData
{
    float2 offset;
    float scale;
    float level;
};

// Vertex shader takes two inputs: vertex position and color.
// By convention, Diligent Engine expects vertex shader inputs to be 
// labeled 'ATTRIBn', where n is the attribute number.
struct VSInput
{
    float2 Pos   : ATTRIB0;
};

struct PSInput 
{ 
    float4 Pos   : SV_POSITION;
};

// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be indentical.
void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    float2 LocalPosXZ = float2(VSIn.Pos.x, VSIn.Pos.y) * scale;
    float2 WPosXZ = LocalPosXZ + offset;
    float4 WPos = float4(WPosXZ.x, 0.0, WPosXZ.y, 1.0);    
    PSIn.Pos = mul(g_WorldViewProj, WPos);
}
