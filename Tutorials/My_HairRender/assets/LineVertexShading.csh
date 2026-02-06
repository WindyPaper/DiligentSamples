//Calculate hair vertex shading data

#include "CommonCS.csh"
#include "HairBsdf.csh"

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

Texture3D<float4> DSLut3D;
SamplerState DSLut3D_sampler;

// groupshared uint GroupNumAccum;
float3 FromLinearAbsorption(float3 In) { return sqrt(In);  }

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
            float3 view_dir = normalize(CameraWPos.xyz - V1.Pos);

            FGBufferData hair_test_gb;
            hair_test_gb.BaseColor = float3(0.8f, 0.8f, 0.8f);
            hair_test_gb.Roughness = 0.1f;
            FHairTransmittanceData TransmittanceData = InitHairStrandsTransmittanceData();
	        TransmittanceData.bUseSeparableR = true;
            float3 V = view_dir;
            float3 L = normalize(DirectionLightDir.xyz);
            float3 T = tangent;            

            const float SinLightAngle = dot(L, T);
            const float3 HairColor = float3(0.8f, 0.8f, 0.8f);
            const float3 RemappedAbsorption = FromLinearAbsorption(HairColor);
            float3 sample_uv0 = float3(saturate(abs(SinLightAngle)), saturate(hair_test_gb.Roughness), saturate(RemappedAbsorption.x));
            float3 sample_uv1 = float3(saturate(abs(SinLightAngle)), saturate(hair_test_gb.Roughness), saturate(RemappedAbsorption.y));
            float3 sample_uv2 = float3(saturate(abs(SinLightAngle)), saturate(hair_test_gb.Roughness), saturate(RemappedAbsorption.z));
            float2 pre_compute_scatter_data_r = DSLut3D.SampleLevel(DSLut3D_sampler, sample_uv0, 0).xy;
            float2 pre_compute_scatter_data_g = DSLut3D.SampleLevel(DSLut3D_sampler, sample_uv1, 0).xy;
            float2 pre_compute_scatter_data_b = DSLut3D.SampleLevel(DSLut3D_sampler, sample_uv2, 0).xy;

            float3 A_front = float3(pre_compute_scatter_data_r.x, pre_compute_scatter_data_g.x, pre_compute_scatter_data_b.x);
            float3 A_back = float3(pre_compute_scatter_data_r.y, pre_compute_scatter_data_g.y, pre_compute_scatter_data_b.y);

            TransmittanceData = ComputeDualScatteringTerms(hair_test_gb.Roughness, V, L, T, A_front, A_back);
    
            float3 hair_dir_fs = HairShading( hair_test_gb, L, V, T, 1.0f, TransmittanceData, 0, 0, 0 );
            OutHairVertexShadeData[VertexIdx0 + 1] = PackR11G11B10F(hair_dir_fs);

            if(VertexType == 1)
            {
                OutHairVertexShadeData[VertexIdx0] = PackR11G11B10F(float3(1.0f, 1.0f, 1.0f));
            }
        }
    }
}
