#include "HairRender.h"

#include "MapHelper.hpp"


Diligent::HairRender::HairRender(IDeviceContext* pDeviceCtx, IRenderDevice* pDevice, IShaderSourceInputStreamFactory* pShaderFactory, ISwapChain *pSwapChain) :
IBaseRender(pDeviceCtx, pDevice, pShaderFactory),
m_pSwapChain(pSwapChain)
{
}

template <typename T, typename Alloc>
inline int SizeOfType(const std::vector<T, Alloc> & v)
{
    return int(sizeof(std::vector<T, Alloc>::value_type));
}

void Diligent::HairRender::InitPSO()
{
    //init common gpu data
    m_HairConstData = CreateConstBuffer(sizeof(HairConstData), nullptr, "Hair const data");
    
    CreateHWPSO();
    CreateDownSampleMapPSO();
}

void Diligent::HairRender::CreateDownSampleMapPSO()
{
    AutoPtrShader ap_downsamplecs = CreateShader("CSMain", "./DownsampledDepthCS.csh", \
        "Down Sample Depth CS", SHADER_TYPE_COMPUTE);

    ComputePipelineStateCreateInfo PSOCreateInfo;
    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    std::vector<std::string> ParamNames = {"HairConstData", "InputSrcDepthMap", "OutputMipDepthMap"};
    std::vector<ShaderResourceVariableDesc> VarsVec = GenerateCSDynParams(ParamNames);
    // clang-format on
    PSOCreateInfo.PSODesc = CreatePSODescAndParam(&VarsVec[0], (int)VarsVec.size(), "Down Sample Depth PSO");	

    PSOCreateInfo.pCS = ap_downsamplecs;
    m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_DownSamleDepthPassCS.PSO);

    //SRB
    m_DownSamleDepthPassCS.PSO->CreateShaderResourceBinding(&m_DownSamleDepthPassCS.SRB, true);

    //data
    float2 PixelSize = float2(m_pSwapChain->GetDesc().Width, m_pSwapChain->GetDesc().Height);
    
    TextureDesc DownSampledTextureDesc;
    DownSampledTextureDesc.Name = "Down Sampled Texture Desc Texture";
    DownSampledTextureDesc.Type = RESOURCE_DIM_TEX_2D;
    DownSampledTextureDesc.Width = Uint32(ceil(PixelSize.x / 16));
    DownSampledTextureDesc.Height = Uint32(ceil(PixelSize.y / 16));
    DownSampledTextureDesc.Format = TEX_FORMAT_R32_FLOAT;
    DownSampledTextureDesc.Usage = USAGE_DYNAMIC;
    DownSampledTextureDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    m_pDevice->CreateTexture(DownSampledTextureDesc, nullptr, &m_DownSamleDepthPassCS.DownSampledDepthMap);

    m_DownSamleDepthPassCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutputMipDepthMap")->Set(\
        m_DownSamleDepthPassCS.DownSampledDepthMap->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));

    m_DownSamleDepthPassCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "HairConstData")->Set(\
        m_HairConstData);

    ITexture *pDepth = m_pSwapChain->GetDepthTexture();
    
    // StateTransitionDesc Barrier{pDepth, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_DEPTH_READ, true};
    // m_pDeviceCtx->TransitionResourceStates(1, &Barrier);
    
    m_DownSamleDepthPassCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "InputSrcDepthMap")->Set(\
        pDepth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

    
}

void Diligent::HairRender::CreateHWPSO()
{
    RefCntAutoPtr<IShader> ap_HWRenderShaderVS = CreateShader("main", "./HairHWVS.vsh", \
        "Hair HW Render VS", SHADER_TYPE_VERTEX);
    RefCntAutoPtr<IShader> ap_HWRenderShaderPS = CreateShader("main", "./HairHWPS.psh", \
        "Hair HW Render PS", SHADER_TYPE_PIXEL);

    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "Hair HW Render PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_LINE_LIST;
    // Cull back faces
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    // Enable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    // clang-format on

    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] = 
    {
        {SHADER_TYPE_VERTEX, "VertexIdxArray", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX, "VertexDataArray", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);
    

    PSOCreateInfo.pVS = ap_HWRenderShaderVS;
    PSOCreateInfo.pPS = ap_HWRenderShaderPS;

    // Define variable type that will be used by default
    //PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_apHWRenderPSO);

    m_VSConstants = CreateConstBuffer(sizeof(float4x4), nullptr, "Hair VS constants CB");
    
    m_apHairIdxArray = CreateStructureBuffer(SizeOfType(m_HairRawData.HairIdxDataArray), \
        (int)m_HairRawData.HairIdxDataArray.size(), \
        &m_HairRawData.HairIdxDataArray[0], \
        "HairIdxBuffer");
    
    m_apHairVertexArray = CreateStructureBuffer(SizeOfType(m_HairRawData.HairVertexDataArray), \
        (int)m_HairRawData.HairVertexDataArray.size(), \
        &m_HairRawData.HairVertexDataArray[0], \
        "HairVertexBuffer");
    
    // Since we did not explcitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
    // change and are bound directly through the pipeline state object.
    //m_apHWRenderPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    IShaderResourceVariable *pStaticVar = m_apHWRenderPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants");
    if(pStaticVar)
    {
        pStaticVar->Set(m_VSConstants);
    }
    //m_apHWRenderPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VertexIdxArray")->Set(m_apHairIdxArray->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    //m_apHWRenderPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VertexDataArray")->Set(m_apHairVertexArray->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

    // Create a shader resource binding object and bind all static resources in it
    m_apHWRenderPSO->CreateShaderResourceBinding(&m_apHWRenderSRB, true);

    m_apHWRenderSRB->GetVariableByName(SHADER_TYPE_VERTEX, "VertexIdxArray")->Set(m_apHairIdxArray->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_apHWRenderSRB->GetVariableByName(SHADER_TYPE_VERTEX, "VertexDataArray")->Set(m_apHairVertexArray->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
}

void Diligent::HairRender::HWRender(const float4x4 &WVPMat)
{
    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<float4x4> CBConstants(m_pDeviceCtx, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        *CBConstants = WVPMat.Transpose();
    }

    // Set the pipeline state
    m_pDeviceCtx->SetPipelineState(m_apHWRenderPSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    m_pDeviceCtx->CommitShaderResources(m_apHWRenderSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    //106774
    DrawAttribs DrawInstAttr;
    DrawInstAttr.NumVertices = 106774;
    DrawInstAttr.NumInstances = 1;
    DrawInstAttr.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pDeviceCtx->Draw(DrawInstAttr);
}

void Diligent::HairRender::RunDownSampledDepthMapCS()
{
    m_pDeviceCtx->SetPipelineState(m_DownSamleDepthPassCS.PSO);
    
    m_pDeviceCtx->CommitShaderResources(m_DownSamleDepthPassCS.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    DispatchComputeAttribs attr(m_DownSamleDepthPassCS.DownSampledDepthMap->GetDesc().Width, \
        m_DownSamleDepthPassCS.DownSampledDepthMap->GetDesc().Height);
    m_pDeviceCtx->DispatchCompute(attr);
}

void Diligent::HairRender::RunCS()
{
    float2 PixelSize = float2(m_pSwapChain->GetDesc().Width, m_pSwapChain->GetDesc().Height);
    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<HairConstData> CBConstants(m_pDeviceCtx, m_HairConstData, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants->ScreenSize = PixelSize;
    }

    RunDownSampledDepthMapCS();
}
