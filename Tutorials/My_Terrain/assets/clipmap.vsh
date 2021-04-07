Texture2D    g_Texture;
SamplerState g_Texture_sampler; // By convention, texture samplers must use the '_sampler' suffix

struct Dimension
{
	float4 Min;
	float4 Size;
};

cbuffer TerrainDimension
{
	Dimension g_TerrainInfo;	
};

cbuffer Constants
{
    float4x4 g_ViewProj;
    float4 g_MorphK;

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
    float2 UV  : TEX_COORD;
};

// morphs vertex xy from from high to low detailed mesh position
float2 MorphVertex( float2 InPos, float2 vertex )
{
   float2 fracPart = (frac( InPos.xy / 2.0f ) * 2.0f) * Scale.xy;
   return vertex.xy - fracPart * g_MorphK.x;
}

// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be indentical.
void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    float2 WPosXZ = float2(VSIn.Pos.x, VSIn.Pos.y) * Scale.xy;

    //vertex morph
    WPosXZ = MorphVertex(VSIn.Pos.xy, WPosXZ);

    float3 WPos = float3(WPosXZ.r, 0.0f, WPosXZ.g) + float3(Offset.x, Offset.y, Offset.z);

    WPos.y = 0.0f;    
    float2 TerrainMapUV = (WPos.xz - g_TerrainInfo.Min.xz) / g_TerrainInfo.Size.xz;
    WPos.y = g_Texture.SampleLevel(g_Texture_sampler, TerrainMapUV, 0).x * g_TerrainInfo.Size.y + g_TerrainInfo.Min.y;
    //WPos.y = 0.0f;

    PSIn.Pos = mul(g_ViewProj, float4(WPos, 1.0f));
    PSIn.UV = TerrainMapUV;
}
