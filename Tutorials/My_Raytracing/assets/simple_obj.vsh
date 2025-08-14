#include "simple_obj_common.csh"

// Vertex shader takes two inputs: vertex position and color.
// By convention, Diligent Engine expects vertex shader inputs to be 
// labeled 'ATTRIBn', where n is the attribute number.
struct VSInput
{
    float4 Pos   : ATTRIB0;
    float4 Normal  : ATTRIB1;
    float2 UV0  : ATTRIB2;
    float2 UV1  : ATTRIB3;
};

struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float2 UV  : TEX_COORD; 
    float4 PixelWPos : TEX_COORD1;
    float3 WNormal : TEX_COORD2;
    float3 PixelViewPos : TEX_COORD3;
    float4 ClipPos : TEX_COORD4;
};

// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be indentical.
void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    float3 local_pos_offset = VSIn.Pos.xyz + float3(0.0f, TestPlaneOffsetY, 0.0f);
    //PSIn.Pos = mul( float4(local_pos_offset,1.0), g_WorldViewProj);
    float4 view_pos = mul(float4(local_pos_offset,1.0), g_ViewMat);
    PSIn.Pos = mul(view_pos, g_ProjMat);
    PSIn.UV  = VSIn.UV0;
    PSIn.PixelWPos = float4(local_pos_offset, PSIn.Pos.z);
    PSIn.PixelViewPos = mul(float4(local_pos_offset, 1.0f), g_ViewMat);
    PSIn.WNormal = VSIn.Normal.xyz;
    PSIn.ClipPos = PSIn.Pos;
}
