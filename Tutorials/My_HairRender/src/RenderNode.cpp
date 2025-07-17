#include "RenderNode.h"

#include "EngineFactory.h"
#include "RefCntAutoPtr.hpp"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"

using namespace Diligent;

RenderPassNode::RenderPassNode(IDeviceContext *pDC) :
    pDeviceContext(pDC)
{
    
}

RenderPassNode& RenderPassNode::Clear(ISwapChain *pSwapChain)
{
    auto* pRTV = pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = pSwapChain->GetDepthBufferDSV();
    // Clear the back buffer
    const float ClearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    pDeviceContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pDeviceContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    return *this;
}

IBaseRender::IBaseRender(IDeviceContext* pDeviceCtx, IRenderDevice* pDevice, IShaderSourceInputStreamFactory* pShaderFactory) :
m_pDeviceCtx(pDeviceCtx),
m_pDevice(pDevice),
m_pShaderFactory(pShaderFactory)
{
    
}

RefCntAutoPtr<IShader> IBaseRender::CreateShader(const std::string& entryPoint, const std::string& csFile, const std::string& descName, const SHADER_TYPE type, ShaderMacroHelper* pMacro)
{
    ShaderCreateInfo ShaderCI;
    ShaderCI.pShaderSourceStreamFactory = m_pShaderFactory;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.UseCombinedTextureSamplers = true;

    /*ShaderMacroHelper Macros;
    Macros.AddShaderMacro("PCG_TILE_MAP_NUM", 8);
    Macros.Finalize();*/

    RefCntAutoPtr<IShader> pShader;
    {
        ShaderCI.Desc.ShaderType = type;
        ShaderCI.EntryPoint = entryPoint.c_str();		
        ShaderCI.FilePath = csFile.c_str();
        ShaderCI.Desc.Name = descName.c_str();

        if (pMacro)
        {
            ShaderCI.Macros = (*pMacro);
        }		

        m_pDevice->CreateShader(ShaderCI, &pShader);
    }

    return pShader;
}

PipelineStateDesc IBaseRender::CreatePSODescAndParam(ShaderResourceVariableDesc* params, const int varNum, const std::string& psoName, const PIPELINE_TYPE type)
{
    PipelineStateDesc PSODesc;

    // This is a compute pipeline
    PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
	
    PSODesc.ResourceLayout.Variables = params;
    PSODesc.ResourceLayout.NumVariables = varNum;

    PSODesc.Name = psoName.c_str();

    return PSODesc;
}

RefCntAutoPtr<IBuffer> IBaseRender::CreateConstBuffer(const int size, void* pInitialData, const std::string& desc)
{
    BufferDesc CBDesc;
    CBDesc.Name           = desc.c_str();
    CBDesc.uiSizeInBytes  = size;
    CBDesc.Usage          = USAGE_DYNAMIC;
    CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
    CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

    BufferData InitialData;
    if (pInitialData != nullptr)
    {
        CBDesc.CPUAccessFlags = CPU_ACCESS_NONE;
        CBDesc.Usage         = USAGE_IMMUTABLE;
        InitialData.pData    = pInitialData;
        InitialData.DataSize = size;
    }
    
    RefCntAutoPtr<IBuffer> apConstBuffer;
    m_pDevice->CreateBuffer(CBDesc, pInitialData ? &InitialData : nullptr, &apConstBuffer);

    return apConstBuffer;
}

RefCntAutoPtr<IBuffer> IBaseRender::CreateStructureBuffer(const int element_size, const int count, void* pInitialData, const std::string &desc)
{
    BufferDesc buf_desc;
    buf_desc.Name              = desc.c_str();
    buf_desc.Usage             = USAGE_DYNAMIC;
    buf_desc.BindFlags         = BIND_SHADER_RESOURCE;
    buf_desc.Mode              = BUFFER_MODE_STRUCTURED;
    buf_desc.CPUAccessFlags    = CPU_ACCESS_WRITE;
    buf_desc.ElementByteStride = element_size;
    buf_desc.uiSizeInBytes     = element_size * count;

    BufferData InitialData;
    if(pInitialData)
    {
        buf_desc.CPUAccessFlags = CPU_ACCESS_NONE;
        buf_desc.Usage         = USAGE_IMMUTABLE;
        InitialData.pData    = pInitialData;
        InitialData.DataSize = buf_desc.uiSizeInBytes;
    }
    
    RefCntAutoPtr<IBuffer> apStructureBuffer;
    m_pDevice->CreateBuffer(buf_desc, pInitialData ? &InitialData : nullptr, &apStructureBuffer);

    return apStructureBuffer;
}

