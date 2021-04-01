cbuffer Constants
{
    float4x4 g_ViewProj;
};

cbuffer PerPatchData
{
    float4 Scale;
    float4 Offset;
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
    float2 WPosXZ = float2(VSIn.Pos.x, VSIn.Pos.y) * Scale.xy;
    float3 WPos = float3(WPosXZ.r, 0.0f, WPosXZ.g) + Offset.xyz;
    PSIn.Pos = mul(g_ViewProj, float4(WPos, 1.0f));
}
