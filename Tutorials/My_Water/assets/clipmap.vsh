Texture2D    g_displacement_texL0;
SamplerState g_displacement_texL0_sampler; // By convention, texture samplers must use the '_sampler' suffix

Texture2D    g_displacement_texL1;
SamplerState g_displacement_texL1_sampler; // By convention, texture samplers must use the '_sampler' suffix

Texture2D    g_displacement_texL2;
SamplerState g_displacement_texL2_sampler; // By convention, texture samplers must use the '_sampler' suffix


// static const float WaterTextureScale = 5.0;

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
    float4 g_CameraPos;

    float2 g_L_FFTScale; //x:L  y:Scale
    float g_BaseNormalIntensity;
    float g_FFTN;

    float LengthScale0;
    float LengthScale1;
    float LengthScale2;
    float Padding;
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
    float3 Normal : TEX_COORD1;
    float3 WorldPos : TEX_COORD2;
    float3 CamPos : TEX_COORD3;
    float2 L_RepeatScale : TEX_COORD4;
    float2 BaseNormalIntensity_N : TEX_COORD5;
};

// morphs vertex xy from from high to low detailed mesh position
float2 MorphVertex( float2 InPos, float2 vertex, float morphk)
{
   float2 fracPart = (frac( InPos / 2.0f ) * 2.0f) * Scale.xy;
   return vertex - fracPart * morphk;
}

// float GetHeightMapOffsetValue(float2 BaseUV, float2 OffsetPixel)
// {
//     float2 OffsetUV = OffsetPixel / 255 * WaterTextureScale;   
//     return g_displacement_tex.SampleLevel(g_displacement_tex_sampler, BaseUV + OffsetUV, 0).y;
// }

// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be indentical.
void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    const float TestScale = 1.0f / 1.0f;

    float2 WPosXZ = float2(VSIn.Pos.x, VSIn.Pos.y) * Scale.xy;    

    float3 WPos = float3(WPosXZ.r, 0.0f, WPosXZ.g) + float3(Offset.x, Offset.y, Offset.z);
    float3 UndisplaceWPos = float3(WPosXZ.r, 0.0f, WPosXZ.g) + float3(Offset.x, Offset.y, Offset.z);

    float WaterTextureScale = g_L_FFTScale.y;

    WPos.y = 0.0f;    
    float2 TerrainMapUV = (WPos.xz - g_TerrainInfo.Min.xz) * WaterTextureScale;
    float baseh = g_displacement_texL0.SampleLevel(g_displacement_texL0_sampler, TerrainMapUV / LengthScale0 * TestScale, 0).y;
    WPos.y = baseh * g_TerrainInfo.Size.y / g_L_FFTScale.x + g_TerrainInfo.Min.y;
    //WPos.y = 0.0f;

    //vertex morph
    float eyeDist     = distance( WPos, g_CameraPos.xyz );
    float morphLerpK  = 1.0f - clamp( g_MorphK.x - eyeDist * g_MorphK.y, 0.0, 1.0 );
    WPos.xz = MorphVertex(VSIn.Pos.xy, WPos.xz, morphLerpK);

    //recalculate by new xz position
    TerrainMapUV = (WPos.xz - g_TerrainInfo.Min.xz) * WaterTextureScale;
    float3 WaterVertexOffset = g_displacement_texL0.SampleLevel(g_displacement_texL0_sampler, TerrainMapUV / LengthScale0 * TestScale, 0).rgb;
    WPos.y = WaterVertexOffset.y + g_TerrainInfo.Min.y;
    WPos.xz += WaterVertexOffset.xz;;
    //WPos = WaterVertexOffset * g_TerrainInfo.Size + g_TerrainInfo.Min;

    //test
    //WPos = UndisplaceWPos + g_displacement_texL0.SampleLevel(g_displacement_texL0_sampler, TerrainMapUV, 0).rgb * (2000.0 / g_L_FFTScale.x);

    PSIn.Pos = mul(g_ViewProj, float4(WPos, 1.0f));
    PSIn.UV = TerrainMapUV;
    //PSIn.Morph = float2(morphLerpK, 1.0f);

    PSIn.WorldPos = WPos;
    PSIn.CamPos = g_CameraPos.xyz;

    PSIn.L_RepeatScale = g_L_FFTScale.xy;
    PSIn.BaseNormalIntensity_N = float2(g_BaseNormalIntensity, g_FFTN);

    //Normal
    // float2 size = float2(2.0,0.0);
    // float3 offset = float3(-1,0,1);
    // float s11 = baseh;
    // float s01 = GetHeightMapOffsetValue(TerrainMapUV, offset.xy);
    // float s21 = GetHeightMapOffsetValue(TerrainMapUV, offset.zy);
    // float s10 = GetHeightMapOffsetValue(TerrainMapUV, offset.yx);
    // float s12 = GetHeightMapOffsetValue(TerrainMapUV, offset.yz);
    // float3 va = normalize(float3(size.x, s21-s01, size.y));
    // float3 vb = normalize(float3(size.y,s12-s10, size.x));
    // float4 bump = float4( cross(va,vb), s11 );

    // PSIn.Normal = bump.xyz;
}
