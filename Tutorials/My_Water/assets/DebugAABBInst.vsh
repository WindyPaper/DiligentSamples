cbuffer Constants
{
    float4x4 g_ViewProj;
};

struct VSInput
{
    // Vertex attributes
    float3 Pos      : ATTRIB0; 

    // Instance attributes
    float4 Scale : ATTRIB1;
    float4 Offset : ATTRIB2;
};

struct PSInput 
{
    float4 Pos : SV_POSITION;
};

// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be indentical.
void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    float4 WPos = float4(VSIn.Pos, 1.0) * VSIn.Scale + VSIn.Offset;
    // Apply view-projection matrix
    PSIn.Pos = mul(g_ViewProj, WPos);
}
