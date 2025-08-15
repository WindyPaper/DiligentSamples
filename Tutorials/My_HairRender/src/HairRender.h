#pragma once

#include "HairData.h"
#include "RenderNode.h"
#include "SwapChain.h"

namespace Diligent
{

struct IShaderSourceInputStreamFactory;
struct IRenderDevice;
struct IDeviceContext;

struct HairConstData
{
    float4x4 ViewProj;
    float4x4 InvViewProj;
    float4 HairBBoxMin;
    float4 HairBBoxSize;
    float2 ScreenSize;
    float2 DownSampleDepthSize;
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
    AutoPtrBuffer RenderQueueBuffer;
    AutoPtrBuffer VisibilityBitBuffer;
    AutoPtrBuffer LineSizeBuffer;
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

    void HWRender(const float4x4 &WVPMat);

    void RunDownSampledDepthMapCS();
    void RunDrawLineCS();
    void RunFrustumVoxelCullLineSizeCS();
    void RunGetLineOffsetAndCounterCS();
    
    void RunCS(const float4x4 &viwe_proj, const float4x4 &inv_view_proj);

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
    uint2 m_DownSampledDepthSize;

    //--Cull start
    //Downsample DepthMap
    DepthDownSamplePassDataCS m_DownSamleDepthPassCS;
    DrawLinePassDataCS m_DrawLinePassCS; // for testing draw line algorithm
    //Calculate line size in frustum voxel
    LineSizeInFrustumVoxelCS m_LineSizeInFrustumVoxelCS;
    GetLineOffsetCounterCS m_GetLineOffsetCounterCS;
    GetLineVisibilityCS m_GetLineVisibilityCS;

    //--Cull end
    
    RefCntAutoPtr<ISwapChain> m_pSwapChain;
};

}
