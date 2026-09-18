#pragma once

#include "HairData.h"
#include "RenderNode.h"
#include "SwapChain.h"

namespace Diligent
{

struct IShaderSourceInputStreamFactory;
struct IRenderDevice;
struct IDeviceContext;

const int MAX_HAIR_LINE_NUM = std::pow(2, 25);

#define SET_SHADER_PARAM_SAFE(P, V) \
    if(P) \
        P->Set(V); \

struct HairConstData
{
    float4x4 ViewProj;
    float4x4 InvViewProj;
    float4 HairBBoxMin;
    float4 HairBBoxToCamMinMaxDist;
    float2 ScreenSize;
    float2 DownSampleDepthSize;
    float4 CameraForward;
    float4 CameraWPos;
};

struct ShadingLightData
{
	float4 DirectionLightDir;
	float4 DirectionLightColor;
	float3 HairColor;
	float  HairRoughness;
	float  HairAlpha;
	float  HairUseRefMarschner;
	float  HairEnableMultiScattering;
	float  HairEnableDeepShadowScattering;

	ShadingLightData()
	{
		DirectionLightDir   = float4(0.5f, 0.5f, 0.5f, 1.0f);
		DirectionLightColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

		HairColor     = float3(0.8f, 0.8f, 0.8f);
		HairRoughness = 0.4f;
			HairAlpha     = 0.07f;
			HairUseRefMarschner = 0.0f;
			HairEnableMultiScattering = 1.0f;
			HairEnableDeepShadowScattering = 1.0f;
	}
};

struct PrecomputeLUTData
{
	uint AbsorptionCount;
	uint RoughnessCount;
	uint ThetaCount;
	uint SampleCountScale;
};

// Mirrors cbuffer DSVolumeInfo in GenerateDSVolumeTexture.csh / LineVertexShading.csh
// Row order matches DSDepthGenerate.dxil SSBO layout so shader indices line up.
struct DSVolumeInfoCB
{
	float4 mMinAABB;         // [0] xyz = min corner
	float4 mMaxAABB;         // [1] xyz = max corner
	uint4  mResolution;      // [2] xyz = voxel resolution
	uint4  mClearResolution; // [3] xyz = clear resolution (== mResolution)
	float4 mScale;           // [4] xyz = thread->voxel scale (DXIL _m0[4], {1,1,1})
	float4 mInvLength;       // [5] xyz = 1 / (max-min)
	float4 mInvResolution;   // [6] xyz = 1 / resolution
};

// Mirrors cbuffer DSVolumeSceneInfo in GenerateDSVolumeTexture.csh
struct DSVolumeSceneInfoCB
{
	float4x4 ViewProj;
	float4x4 InvViewProj;
	float4   ViewDepthAxis;
	float4   DepthSize;
	float4   Tolerance;
};

// Mirrors cbuffer DownsampleInfo in DepthVolumeDownsample.csh
struct DownsampleInfoCB
{
	uint4 DstMipLevel;
};

// Mirrors cbuffer DSInfo in GenerateDSVolumeTexture.csh (CSGenerateFromHair).
// Field order is the reference DXIL _33 layout (see GenerateDSVolumeTextureFromHair.hlsl):
//   row0 (VoxelWorldSize, VolumeResolution, VolumePageResolution, RasterDepthThreshold)
//   row1 (LUTThetaCount, LUTRoughnessCount, LUTAbsorptionCount, VolumeTracingOffsetScale)
//   row2 (VolumeTracingDelta, RasterStrandWidthScale, StrandWidthMin, StrandWidthMax)
//   row3 (StrandWidthAve, VolumeTracingIBLDelta, <BackscatterScale>, pad)
// z of row3 is a pad slot in the reference; we reuse it for the material's
// backscatter scale (the reference reads that from the material cbuffer).
struct DSInfoCB
{
	float4 Row0;   // x=VoxelWorldSize y=VolumeResolution z=VolumePageResolution w=RasterDepthThreshold
	float4 Row1;   // x=LUTThetaCount y=LUTRoughnessCount z=LUTAbsorptionCount w=VolumeTracingOffsetScale
	float4 Row2;   // x=VolumeTracingDelta y=RasterStrandWidthScale z=StrandWidthMin w=StrandWidthMax
	float4 Row3;   // x=StrandWidthAve y=VolumeTracingIBLDelta z=BackscatterScale
};

// Mirrors cbuffer HairStrandCountInfo in GenerateDSVolumeTexture.csh.
struct HairStrandCountCB
{
	uint4 HairStrandCount;   // x = number of strands, y = leading strands to voxelize
};

struct PassBaseData
{
    RefCntAutoPtr<IPipelineState>         PSO;
    RefCntAutoPtr<IShaderResourceBinding> SRB;
};

struct DepthDownSamplePassDataCS : public PassBaseData
{
    AutoPtrTex DownSampledDepthMap;
};

struct DrawLinePassDataCS : public PassBaseData
{
    AutoPtrBuffer VerticesData;
    AutoPtrBuffer LineIdxData;

    AutoPtrBuffer DrawLineQueue;

    AutoPtrTex DrawLineTex;
};

struct LineSizeInFrustumVoxelCS : public PassBaseData
{
    AutoPtrBuffer VerticesData;
    AutoPtrBuffer LineIdxData;

    AutoPtrBuffer LineSizeBuffer;
};

struct GetLineOffsetCounterCS : public  PassBaseData
{
    AutoPtrBuffer LineSizeBuffer;
    
    AutoPtrBuffer CounterBuffer;
    AutoPtrBuffer LineOffsetBuffer;

    AutoPtrBuffer CountStageBuffer;
    std::vector<uint> CountCPUData;
};

struct GetLineVisibilityCS : public PassBaseData
{
    AutoPtrBuffer VerticesData;
    AutoPtrBuffer LineIdxData;
    
    AutoPtrBuffer LineOffsetBuffer;

    AutoPtrBuffer RenderQueueBuffer;
    AutoPtrBuffer VisibilityBitBuffer;
    AutoPtrBuffer LineSizeBuffer;
};

struct GetWorkQueueCS : public PassBaseData
{
    AutoPtrBuffer LineSizeBuffer;
    AutoPtrBuffer WorkQueueBuffer;
    AutoPtrBuffer WorkQueueCountBuffer;

    AutoPtrBuffer RenderQueueBuffer;

    AutoPtrBuffer CountStageBuffer;
    std::vector<uint> CountCPUData;
};

struct DrawLineFromWorkQueueCS : public PassBaseData
{
    AutoPtrBuffer VerticesData;
    AutoPtrBuffer LineIdxData;
    
    AutoPtrBuffer LineOffsetBuffer;
    AutoPtrBuffer WorkQueueBuffer;
    AutoPtrBuffer LineSizeBuffer;
    AutoPtrBuffer RenderQueueBuffer;
    AutoPtrBuffer HairVertexShadeData;
    
    //AutoPtrTex OutHairRenderTex;
    AutoPtrTex OutDebugLayerTex;
    AutoPtrTex OutDebugLayerInfoTex0;
    AutoPtrTex OutDebugLayerInfoTex1;
    AutoPtrTex OutDebugLayerInfoTex2;
    AutoPtrTex OutDebugLayerInfoTex3;
};

struct PrecomputeLUTForShadingCS : public PassBaseData
{
	AutoPtrBuffer PrecomputeLUTData;
	AutoPtrBuffer PrecomputeLUTDataNTT;
	AutoPtrTex    OutHairAveragePrecomputeData;
	AutoPtrTex    OutHairNTTPrecomputeData;

	RefCntAutoPtr<IPipelineState>         PSO_NTT;
	RefCntAutoPtr<IShaderResourceBinding> SRB_NTT;
};

struct GenerateDSVolumeCS : public PassBaseData
{
	AutoPtrTex    DSVolumeTexture;
	AutoPtrBuffer DSVolumeInfoBuffer;
	AutoPtrBuffer DSVolumeSceneInfoBuffer;

	// FromHair pass: splats hair strand density into the volume.
	RefCntAutoPtr<IPipelineState>         PSO_FromHair;
	RefCntAutoPtr<IShaderResourceBinding> SRB_FromHair;
	AutoPtrBuffer DSInfoBuffer;
	AutoPtrBuffer StrandCountBuffer;
	AutoPtrBuffer VerticesData;
	AutoPtrBuffer LineIdxData;

	RefCntAutoPtr<IPipelineState>         PSO_Clear;
	RefCntAutoPtr<IShaderResourceBinding> SRB_Clear;

	// Mip-chain downsample
	RefCntAutoPtr<IPipelineState>                       PSO_Downsample;
	std::vector<RefCntAutoPtr<IShaderResourceBinding>>  SRB_Downsample;   // one per dst mip
	std::vector<AutoPtrBuffer>                          DownsampleInfoBuffers;
	std::vector<RefCntAutoPtr<ITextureView>>            MipSRVs;           // per-mip SRV
	std::vector<RefCntAutoPtr<ITextureView>>            MipUAVs;           // per-mip UAV
};

struct VertexShadingCS : public PassBaseData{
	AutoPtrBuffer VerticesData;
	AutoPtrBuffer LineIdxData;

	AutoPtrBuffer LineOffsetBuffer;
	AutoPtrBuffer WorkQueueBuffer;
	AutoPtrBuffer LineSizeBuffer;
	AutoPtrBuffer RenderQueueBuffer;
	AutoPtrBuffer OutHairVertexShadeData;
};

class HairRender : public IBaseRender
{
public:
    HairRender(IDeviceContext *pDeviceCtx, \
        IRenderDevice *pDevice, \
        IShaderSourceInputStreamFactory *pShaderFactory, \
        ISwapChain *pSwapChain);

    void InitPSO();
    
    void CreateHWPSO();
    
    void CreateDownSampleMapPSO();
    void CreateDrawLinePSO();
    void CreateLineSizeInFrustumVoxelPSO();
    void CreateGetLineOffsetAndCounterPSO();
    void CreateGetLineVisibilityPSO();
	void CreatePrecomputeForShadingPSO();
	void CreateGenerateDSVolumePSO();
	void CreateVertexShadingPSO();
    void CreateGetWorkQueuePSO();
    void CreateDrawLineFromWorkQueueCS();
    //void CreateGetLineVisibilityDependencyPSOParams(int visibility_line_count);

    void HWRender(const float4x4 &WVPMat);

    void RunDownSampledDepthMapCS();
    void RunDrawLineCS();
    void RunFrustumVoxelCullLineSizeCS();
    void RunGetLineOffsetAndCounterCS();
    void RunGetLineVisibilityCS();
	void RunPrecomputeForShadingCS();
	void RunGenerateDSVolumeCS();
	void RunVertexShadingCS();
    void RunGetWorkQueueCS();
    void RunDrawLineFromWorkQueueCS(ITexture *pRTView);
    
    void RunCS(const float4x4 &view_mat, const float4x4 &viwe_proj, const float4x4 &inv_view_proj, \
		ITexture *pRTView, const float3 &cam_forward, const float3 &cam_pos, \
		const ShadingLightData &shading_data);

private:
    HairData m_HairRawData;
    
    //HW Render
    RefCntAutoPtr<IBuffer> m_VSConstants;
    RefCntAutoPtr<IBuffer> m_apHairIdxArray;
    RefCntAutoPtr<IBuffer> m_apHairVertexArray;
    RefCntAutoPtr<IPipelineState>         m_apHWRenderPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_apHWRenderSRB;

    //common
    AutoPtrBuffer m_HairConstData;
	AutoPtrBuffer m_LightData;
    uint2 m_DownSampledDepthSize;

    //--Cull start
    //Downsample DepthMap
    DepthDownSamplePassDataCS m_DownSamleDepthPassCS;
    DrawLinePassDataCS m_DrawLinePassCS; // for testing draw line algorithm
    //Calculate line size in frustum voxel
    LineSizeInFrustumVoxelCS m_LineSizeInFrustumVoxelCS;
    GetLineOffsetCounterCS m_GetLineOffsetCounterCS;
    GetLineVisibilityCS m_GetLineVisibilityCS;
	PrecomputeLUTForShadingCS m_PrecomputeLUTForShadingCS;
	PrecomputeLUTData m_PrecomputeLutConfigData;
	GenerateDSVolumeCS m_GenerateDSVolumeCS;
	VertexShadingCS m_VertexShadingCS;
    int m_VisibilityLineCount;
    GetWorkQueueCS m_GetWorkQueueCS;
    DrawLineFromWorkQueueCS m_DrawLineFromWorkQueueCS;

    //--Cull end
    
    RefCntAutoPtr<ISwapChain> m_pSwapChain;
};

}
