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

struct LightData
{
	float4 DirectionLightDir;
	float4 DirectionLightColor;
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

struct VertexShadingCS : public PassBaseData
{
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
	void RunVertexShadingCS();
    void RunGetWorkQueueCS();
    void RunDrawLineFromWorkQueueCS(ITexture *pRTView);
    
    void RunCS(const float4x4 &view_mat, const float4x4 &viwe_proj, const float4x4 &inv_view_proj, \
		ITexture *pRTView, const float3 &cam_forward, const float3 &cam_pos, \
		const float4 &dir_light_dir, const float4 &dir_color);

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
	VertexShadingCS m_VertexShadingCS;
    int m_VisibilityLineCount;
    GetWorkQueueCS m_GetWorkQueueCS;
    DrawLineFromWorkQueueCS m_DrawLineFromWorkQueueCS;

    //--Cull end
    
    RefCntAutoPtr<ISwapChain> m_pSwapChain;
};

}
