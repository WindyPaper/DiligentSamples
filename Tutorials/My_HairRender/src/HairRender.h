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
    float2 ScreenSize;
    float2 padding;
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

    void HWRender(const float4x4 &WVPMat);

    void RunDownSampledDepthMapCS();
    void RunDrawLineCS();
    void RunFrustumVoxelCullLineSizeCS();
    
    void RunCS();

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

    //--Cull start
    //Downsample DepthMap
    DepthDownSamplePassDataCS m_DownSamleDepthPassCS;
    DrawLinePassDataCS m_DrawLinePassCS; // for testing draw line algorithm
    //Calculate line size in frustum voxel
    LineSizeInFrustumVoxelCS m_LineSizeInFrustumVoxelCS;

    //--Cull end
    
    RefCntAutoPtr<ISwapChain> m_pSwapChain;
};

}
