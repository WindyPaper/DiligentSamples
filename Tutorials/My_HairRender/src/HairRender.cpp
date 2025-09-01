#include "HairRender.h"

#include "MapHelper.hpp"


namespace Diligent {
class DeviceContextD3D12Impl;
}

Diligent::HairRender::HairRender(IDeviceContext* pDeviceCtx, IRenderDevice* pDevice, IShaderSourceInputStreamFactory* pShaderFactory, ISwapChain *pSwapChain) :
IBaseRender(pDeviceCtx, pDevice, pShaderFactory),
m_pSwapChain(pSwapChain),
m_VisibilityLineCount(0)
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
    CreateDrawLinePSO();
    CreateLineSizeInFrustumVoxelPSO();
    CreateGetLineOffsetAndCounterPSO();
    CreateGetLineVisibilityPSO();
    CreateGetWorkQueuePSO();
    CreateDrawLineFromWorkQueueCS();
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

    m_DownSampledDepthSize.x = Uint32(ceil(PixelSize.x / 16));
    m_DownSampledDepthSize.y = Uint32(ceil(PixelSize.y / 16));
    
    TextureDesc DownSampledTextureDesc;
    DownSampledTextureDesc.Name = "Down Sampled Texture Desc";
    DownSampledTextureDesc.Type = RESOURCE_DIM_TEX_2D;
    DownSampledTextureDesc.Width = m_DownSampledDepthSize.x;
    DownSampledTextureDesc.Height = m_DownSampledDepthSize.y;
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

void Diligent::HairRender::CreateDrawLinePSO()
{
    AutoPtrShader ap_drawline = CreateShader("CSMain", "./DrawLine.csh", \
        "Draw Line CS", SHADER_TYPE_COMPUTE);

    ComputePipelineStateCreateInfo PSOCreateInfo;
    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    std::vector<std::string> ParamNames = {"OutputTexture"};//{"HairConstData","OutputTexture"};
    std::vector<ShaderResourceVariableDesc> VarsVec = GenerateCSDynParams(ParamNames);
    
    // clang-format on
    PSOCreateInfo.PSODesc = CreatePSODescAndParam(&VarsVec[0], (int)VarsVec.size(), "Draw line PSO");	

    PSOCreateInfo.pCS = ap_drawline;
    m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_DrawLinePassCS.PSO);

    //SRB
    m_DrawLinePassCS.PSO->CreateShaderResourceBinding(&m_DrawLinePassCS.SRB, true);

    //data
    float2 PixelSize = float2(m_pSwapChain->GetDesc().Width, m_pSwapChain->GetDesc().Height);
    
    TextureDesc DrawLineTexDesc;
    DrawLineTexDesc.Name = "Draw line Texture";
    DrawLineTexDesc.Type = RESOURCE_DIM_TEX_2D;
    DrawLineTexDesc.Width = Uint32(PixelSize.x);
    DrawLineTexDesc.Height = Uint32(PixelSize.y);
    DrawLineTexDesc.Format = TEX_FORMAT_RGBA8_UNORM;
    //DrawLineTexDesc.Usage = USAGE_DYNAMIC;
    DrawLineTexDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    m_pDevice->CreateTexture(DrawLineTexDesc, nullptr, &m_DrawLinePassCS.DrawLineTex);

    m_DrawLinePassCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutputTexture")->Set(\
        m_DrawLinePassCS.DrawLineTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
}

void Diligent::HairRender::CreateLineSizeInFrustumVoxelPSO()
{
    AutoPtrShader ap_drawline = CreateShader("CSMain", "./LineSizeInFrustumVoxelCS.csh", \
        "Line size in frustum voxel CS", SHADER_TYPE_COMPUTE);

    ComputePipelineStateCreateInfo PSOCreateInfo;
    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    std::vector<std::string> ParamNames = { \
        "HairConstData", \
        "VerticesDatas", \
        "IdxData", \
        "DownSampleDepthMap", \
        "OutLineAccumulateBuffer" \
    };
    std::vector<ShaderResourceVariableDesc> VarsVec = GenerateCSDynParams(ParamNames);
    
    // clang-format on
    PSOCreateInfo.PSODesc = CreatePSODescAndParam(&VarsVec[0], (int)VarsVec.size(), "Line Accumulate PSO");	

    PSOCreateInfo.pCS = ap_drawline;
    m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_LineSizeInFrustumVoxelCS.PSO);

    //SRB
    m_LineSizeInFrustumVoxelCS.PSO->CreateShaderResourceBinding(&m_LineSizeInFrustumVoxelCS.SRB, true);

    m_LineSizeInFrustumVoxelCS.VerticesData = m_apHairVertexArray;
    m_LineSizeInFrustumVoxelCS.LineIdxData = m_apHairIdxArray;

    const int VOXEL_SLICE_NUM = 24;
    m_LineSizeInFrustumVoxelCS.LineSizeBuffer = CreateRawBuffer(sizeof(int) * \
        m_DownSampledDepthSize.x * m_DownSampledDepthSize.y * VOXEL_SLICE_NUM, \
        nullptr, \
        "FrustumVoxelLineSizeBuffer");

    m_LineSizeInFrustumVoxelCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "VerticesDatas")->Set(\
        m_LineSizeInFrustumVoxelCS.VerticesData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_LineSizeInFrustumVoxelCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "IdxData")->Set( \
        m_LineSizeInFrustumVoxelCS.LineIdxData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_LineSizeInFrustumVoxelCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "DownSampleDepthMap")->Set( \
        m_DownSamleDepthPassCS.DownSampledDepthMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    m_LineSizeInFrustumVoxelCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutLineAccumulateBuffer")->Set( \
        m_LineSizeInFrustumVoxelCS.LineSizeBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
    m_LineSizeInFrustumVoxelCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "HairConstData")->Set( \
        m_HairConstData);
    
}

void Diligent::HairRender::CreateGetLineOffsetAndCounterPSO()
{
    AutoPtrShader ap_drawline = CreateShader("CSMain", "./LineGetOffsetCounterCS.csh", \
        "Line size to Offset and Counter CS", SHADER_TYPE_COMPUTE);

    ComputePipelineStateCreateInfo PSOCreateInfo;
    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    std::vector<std::string> ParamNames = { \
        "HairConstData", \
        "LineAccumulateBuffer", \
        "OutLineOffsetBuffer", \
        "OutLineCounterBuffer"
    };
    std::vector<ShaderResourceVariableDesc> VarsVec = GenerateCSDynParams(ParamNames);
    
    // clang-format on
    PSOCreateInfo.PSODesc = CreatePSODescAndParam(&VarsVec[0], (int)VarsVec.size(), "Line offset and counter PSO");	

    PSOCreateInfo.pCS = ap_drawline;
    m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_GetLineOffsetCounterCS.PSO);

    //SRB
    m_GetLineOffsetCounterCS.PSO->CreateShaderResourceBinding(&m_GetLineOffsetCounterCS.SRB, true);

    m_GetLineOffsetCounterCS.LineSizeBuffer = m_LineSizeInFrustumVoxelCS.LineSizeBuffer;

    const int VOXEL_SLICE_NUM = 24;
    m_GetLineOffsetCounterCS.LineOffsetBuffer = CreateRawBuffer(sizeof(int) * \
        m_DownSampledDepthSize.x * m_DownSampledDepthSize.y * VOXEL_SLICE_NUM, \
        nullptr, \
        "Line offset buffer");

    uint4 InitUint4 = uint4(0, 0, 0, 0);
    m_GetLineOffsetCounterCS.CounterBuffer = CreateRawBuffer(sizeof(int) * 4, &InitUint4[0], "Counter");
    //m_GetLineOffsetCounterCS.CountStageBuffer = CreateStageStructureBuffer(sizeof(int) * 4, 1, "CounterStage");

    m_GetLineOffsetCounterCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "HairConstData")->Set(m_HairConstData);
    m_GetLineOffsetCounterCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "LineAccumulateBuffer")->Set( \
        m_GetLineOffsetCounterCS.LineSizeBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
    m_GetLineOffsetCounterCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutLineOffsetBuffer")->Set( \
        m_GetLineOffsetCounterCS.LineOffsetBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
    m_GetLineOffsetCounterCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutLineCounterBuffer")->Set( \
        m_GetLineOffsetCounterCS.CounterBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
}

void Diligent::HairRender::CreateGetLineVisibilityPSO()
{
    //uint RenderQueueBufferCount = m_GetLineOffsetCounterCS.CountCPUData[0];
    AutoPtrShader ap_get_line_visibility = CreateShader("CSMain", "./LineGetVisibilityCS.csh", \
        "Line Get Visibility CS", SHADER_TYPE_COMPUTE);

    ComputePipelineStateCreateInfo PSOCreateInfo;
    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    std::vector<std::string> ParamNames = { \
        "HairConstData", \
        "VerticesDatas", \
        "IdxData", \
        "LineOffsetBuffer", \
        "DownSampleDepthMap", \
        "OutLineAccumulateBuffer", \
        "OutLineVisibilityBuffer", \
        "OutLineRenderQueueBuffer"
    };
    std::vector<ShaderResourceVariableDesc> VarsVec = GenerateCSDynParams(ParamNames);
    
    // clang-format on
    PSOCreateInfo.PSODesc = CreatePSODescAndParam(&VarsVec[0], (int)VarsVec.size(), "get line visibility PSO");	

    PSOCreateInfo.pCS = ap_get_line_visibility;
    m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_GetLineVisibilityCS.PSO);

    //SRB
    m_GetLineVisibilityCS.PSO->CreateShaderResourceBinding(&m_GetLineVisibilityCS.SRB, true);

    m_GetLineVisibilityCS.LineSizeBuffer = m_LineSizeInFrustumVoxelCS.LineSizeBuffer;
    m_GetLineVisibilityCS.LineOffsetBuffer = m_GetLineOffsetCounterCS.LineOffsetBuffer;
    m_GetLineVisibilityCS.VerticesData = m_apHairVertexArray;
    m_GetLineVisibilityCS.LineIdxData = m_apHairIdxArray;

    m_GetLineVisibilityCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "HairConstData")->Set(m_HairConstData);
    m_GetLineVisibilityCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "VerticesDatas")->Set( \
        m_GetLineVisibilityCS.VerticesData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_GetLineVisibilityCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "IdxData")->Set( \
        m_GetLineVisibilityCS.LineIdxData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_GetLineVisibilityCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "LineOffsetBuffer")->Set( \
        m_GetLineVisibilityCS.LineOffsetBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_GetLineVisibilityCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "DownSampleDepthMap")->Set( \
        m_DownSamleDepthPassCS.DownSampledDepthMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    m_GetLineVisibilityCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutLineAccumulateBuffer")->Set( \
        m_GetLineVisibilityCS.LineSizeBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

    float LineCount = (float)m_HairRawData.HairIdxDataArray.size();
    m_GetLineVisibilityCS.VisibilityBitBuffer = CreateStructureBuffer(sizeof(int), ceil(LineCount / 32.0f), nullptr, " visibility bit buffer");
    m_GetLineVisibilityCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutLineVisibilityBuffer")->Set( \
        m_GetLineVisibilityCS.VisibilityBitBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
    
    m_GetLineVisibilityCS.RenderQueueBuffer = CreateStructureBuffer(sizeof(int), MAX_HAIR_LINE_NUM, nullptr, " render line queue buffer");
    m_GetLineVisibilityCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutLineRenderQueueBuffer")->Set( \
        m_GetLineVisibilityCS.RenderQueueBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
}

void Diligent::HairRender::CreateGetWorkQueuePSO()
{
    AutoPtrShader ap_cs = CreateShader("CSMain", "./LineGetWorkQueue.csh", \
        "Get work queue CS", SHADER_TYPE_COMPUTE);

    ComputePipelineStateCreateInfo PSOCreateInfo;
    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    std::vector<std::string> ParamNames = { \
        "HairConstData", \
        "LineOffsetBuffer", \
        "OutWorkQueueBuffer", \
        "OutWorkQueueCountBuffer"
    };
    std::vector<ShaderResourceVariableDesc> VarsVec = GenerateCSDynParams(ParamNames);
    
    // clang-format on
    PSOCreateInfo.PSODesc = CreatePSODescAndParam(&VarsVec[0], (int)VarsVec.size(), "Line Get Work Queue PSO");	

    PSOCreateInfo.pCS = ap_cs;
    m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_GetWorkQueueCS.PSO);

    //SRB
    m_GetWorkQueueCS.PSO->CreateShaderResourceBinding(&m_GetWorkQueueCS.SRB, true);

    m_GetWorkQueueCS.LineOffsetBuffer = m_GetLineOffsetCounterCS.LineOffsetBuffer;
    
    m_GetWorkQueueCS.WorkQueueBuffer = CreateStructureBuffer(sizeof(int), \
        m_DownSampledDepthSize.x * m_DownSampledDepthSize.y, \
        nullptr, \
        "Work Queue buffer");

    m_GetWorkQueueCS.WorkQueueCountBuffer = CreateRawBuffer(sizeof(int) * \
        4, \
        nullptr, \
        "Work Queue Count buffer");
    
    m_GetWorkQueueCS.CountStageBuffer = CreateStageStructureBuffer(sizeof(int), \
        4, "Work Queue Counter Stage Buffer");

    m_GetWorkQueueCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "HairConstData")->Set(m_HairConstData);
    m_GetWorkQueueCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "LineOffsetBuffer")->Set( \
        m_GetWorkQueueCS.LineOffsetBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_GetWorkQueueCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutWorkQueueBuffer")->Set( \
        m_GetWorkQueueCS.WorkQueueBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
    m_GetWorkQueueCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutWorkQueueCountBuffer")->Set( \
        m_GetWorkQueueCS.WorkQueueCountBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
}

void Diligent::HairRender::CreateDrawLineFromWorkQueueCS()
{
    AutoPtrShader ap_cs = CreateShader("CSMain", "./DrawLineFromWorkQueueCS.csh", \
        "draw line from work queue CS", SHADER_TYPE_COMPUTE);

    ComputePipelineStateCreateInfo PSOCreateInfo;
    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    std::vector<std::string> ParamNames = { \
        "HairConstData", \
        "VerticesDatas", \
        "IdxData", \
        "WorkQueueBuffer", \
        "LineSizeBuffer", \
        "LineOffsetBuffer", \
        "RenderQueueBuffer", \
        "FullDepthMap", \
        "OutHairRenderTex"
    };
    std::vector<ShaderResourceVariableDesc> VarsVec = GenerateCSDynParams(ParamNames);
    
    // clang-format on
    PSOCreateInfo.PSODesc = CreatePSODescAndParam(&VarsVec[0], (int)VarsVec.size(), "Draw line From Work Queue PSO");	

    PSOCreateInfo.pCS = ap_cs;
    m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_DrawLineFromWorkQueueCS.PSO);

    //SRB
    m_DrawLineFromWorkQueueCS.PSO->CreateShaderResourceBinding(&m_DrawLineFromWorkQueueCS.SRB, true);

    m_DrawLineFromWorkQueueCS.LineOffsetBuffer = m_GetLineOffsetCounterCS.LineOffsetBuffer;
    m_DrawLineFromWorkQueueCS.WorkQueueBuffer = m_GetWorkQueueCS.WorkQueueBuffer;

    m_DrawLineFromWorkQueueCS.VerticesData = m_apHairVertexArray;
    m_DrawLineFromWorkQueueCS.LineIdxData = m_apHairIdxArray;
    
    m_DrawLineFromWorkQueueCS.LineSizeBuffer = m_LineSizeInFrustumVoxelCS.LineSizeBuffer;
    m_DrawLineFromWorkQueueCS.RenderQueueBuffer = m_GetLineVisibilityCS.RenderQueueBuffer;

    //create result buffer
    float2 ScrPixelSize = float2(m_pSwapChain->GetDesc().Width, m_pSwapChain->GetDesc().Height);
    TextureDesc HairResultTextureDesc;
    HairResultTextureDesc.Name = "HairResult Texture Desc";
    HairResultTextureDesc.Type = RESOURCE_DIM_TEX_2D;
    HairResultTextureDesc.Width = ScrPixelSize.x;
    HairResultTextureDesc.Height = ScrPixelSize.y;
    HairResultTextureDesc.Format = TEX_FORMAT_R11G11B10_FLOAT;
    HairResultTextureDesc.Usage = USAGE_DYNAMIC;
    HairResultTextureDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    m_pDevice->CreateTexture(HairResultTextureDesc, nullptr, &m_DrawLineFromWorkQueueCS.OutHairRenderTex);

    ITexture *pDepth = m_pSwapChain->GetDepthTexture();
    
    m_DrawLineFromWorkQueueCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "HairConstData")->Set(m_HairConstData);
    m_DrawLineFromWorkQueueCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "VerticesDatas")->Set( \
        m_DrawLineFromWorkQueueCS.VerticesData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_DrawLineFromWorkQueueCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "IdxData")->Set( \
        m_DrawLineFromWorkQueueCS.LineIdxData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_DrawLineFromWorkQueueCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "WorkQueueBuffer")->Set( \
        m_DrawLineFromWorkQueueCS.WorkQueueBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_DrawLineFromWorkQueueCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "LineSizeBuffer")->Set( \
        m_DrawLineFromWorkQueueCS.LineSizeBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_DrawLineFromWorkQueueCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "LineOffsetBuffer")->Set( \
        m_DrawLineFromWorkQueueCS.LineOffsetBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_DrawLineFromWorkQueueCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "RenderQueueBuffer")->Set( \
        m_DrawLineFromWorkQueueCS.RenderQueueBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    auto pDepthMapParam = m_DrawLineFromWorkQueueCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "FullDepthMap");
    if(pDepthMapParam)
        pDepthMapParam->Set(pDepth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    // m_DrawLineFromWorkQueueCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutHairRenderTex")->Set( \
    //     m_DrawLineFromWorkQueueCS.OutHairRenderTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
}

// void Diligent::HairRender::CreateGetLineVisibilityDependencyPSOParams(int visibility_line_count)
// {
//     if(m_VisibilityLineCount != visibility_line_count)
//     {
//         m_GetLineVisibilityCS.RenderQueueBuffer = CreateStructureBuffer(sizeof(int), visibility_line_count, nullptr, " render line queue buffer");
//
//         m_GetLineVisibilityCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutLineRenderQueueBuffer")->Set( \
//             m_GetLineVisibilityCS.RenderQueueBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
//
//         m_VisibilityLineCount = visibility_line_count;
//     }
// }

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

void Diligent::HairRender::RunDrawLineCS()
{
    m_pDeviceCtx->SetPipelineState(m_DrawLinePassCS.PSO);

    // m_DrawLinePassCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutputTexture")->Set(\
    //     m_DrawLinePassCS.DrawLineTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
    
    m_pDeviceCtx->CommitShaderResources(m_DrawLinePassCS.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    DispatchComputeAttribs attr(m_DrawLinePassCS.DrawLineTex->GetDesc().Width, m_DrawLinePassCS.DrawLineTex->GetDesc().Height);
    m_pDeviceCtx->DispatchCompute(attr);
}

void Diligent::HairRender::RunFrustumVoxelCullLineSizeCS()
{
    m_pDeviceCtx->SetPipelineState(m_LineSizeInFrustumVoxelCS.PSO);

    float LineCount = (float)m_HairRawData.HairIdxDataArray.size();

    m_pDeviceCtx->CommitShaderResources(m_LineSizeInFrustumVoxelCS.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pDeviceCtx->ClearUAVBuffer(m_LineSizeInFrustumVoxelCS.LineSizeBuffer);

    const uint LineSizeCSPerGroupThreadNum = 64;
    
    const DispatchComputeAttribs attr((uint)ceil(LineCount / LineSizeCSPerGroupThreadNum), 1, 1);
    m_pDeviceCtx->DispatchCompute(attr);
    
}

void Diligent::HairRender::RunGetLineOffsetAndCounterCS()
{
    m_pDeviceCtx->SetPipelineState(m_GetLineOffsetCounterCS.PSO);

    m_pDeviceCtx->CommitShaderResources(m_GetLineOffsetCounterCS.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	m_pDeviceCtx->ClearUAVBuffer(m_GetLineOffsetCounterCS.LineOffsetBuffer);
	m_pDeviceCtx->ClearUAVBuffer(m_GetLineOffsetCounterCS.CounterBuffer);
    
    const DispatchComputeAttribs attr(\
        (uint)ceil(m_DownSampledDepthSize.x / 8.0f), \
        (uint)ceil(m_DownSampledDepthSize.y / 8.0f), 1);
    m_pDeviceCtx->DispatchCompute(attr);

    //uint4 ResetCounter = uint4(0, 0, 0, 0);
    //m_pDeviceCtx->UpdateBuffer(m_GetLineOffsetCounterCS.CounterBuffer, 0, sizeof(int) * 4, &ResetCounter, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    //DeviceContextD3D12Impl *pDx12Ctx = reinterpret_cast<DeviceContextD3D12Impl*>(m_pDeviceCtx);

    //read back data to cpu
    // m_pDeviceCtx->CopyBuffer(m_GetLineOffsetCounterCS.CounterBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
    //     m_GetLineOffsetCounterCS.CountStageBuffer, 0, 4 * sizeof(uint),
    //     RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    //
    // //sync gpu finish copy operation.
    // m_pDeviceCtx->WaitForIdle();
    //
    // MapHelper<uint> counter_stage_map_data(m_pDeviceCtx, m_GetLineOffsetCounterCS.CountStageBuffer, MAP_READ, MAP_FLAG_DO_NOT_WAIT);
	   //
    // m_GetLineOffsetCounterCS.CountCPUData.resize(4);
    // memcpy(&m_GetLineOffsetCounterCS.CountCPUData[0], counter_stage_map_data, sizeof(uint) * 4);

    //CreateGetLineVisibilityDependencyPSOParams(m_GetLineOffsetCounterCS.CountCPUData[0]);
}

void Diligent::HairRender::RunGetLineVisibilityCS()
{
    m_pDeviceCtx->SetPipelineState(m_GetLineVisibilityCS.PSO);

    float LineCount = (float)m_HairRawData.HairIdxDataArray.size();

    m_pDeviceCtx->CommitShaderResources(m_GetLineVisibilityCS.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const uint LineSizeCSPerGroupThreadNum = 64;
    
    const DispatchComputeAttribs attr((uint)ceil(LineCount / LineSizeCSPerGroupThreadNum), 1, 1);
    m_pDeviceCtx->DispatchCompute(attr);
}

void Diligent::HairRender::RunGetWorkQueueCS()
{
    m_pDeviceCtx->SetPipelineState(m_GetWorkQueueCS.PSO);

    m_pDeviceCtx->CommitShaderResources(m_GetWorkQueueCS.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pDeviceCtx->ClearUAVBuffer(m_GetWorkQueueCS.WorkQueueCountBuffer);
    
    const DispatchComputeAttribs attr(\
        (uint)ceil(m_DownSampledDepthSize.x / 8.0f), \
        (uint)ceil(m_DownSampledDepthSize.y / 8.0f), \
        1);
    m_pDeviceCtx->DispatchCompute(attr);
}

void Diligent::HairRender::RunDrawLineFromWorkQueueCS(ITexture *pRTView)
{
    //read back data to cpu
    m_pDeviceCtx->CopyBuffer(m_GetWorkQueueCS.WorkQueueCountBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        m_GetWorkQueueCS.CountStageBuffer, 0, 4 * sizeof(uint),
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    //sync gpu finish copy operation.
    m_pDeviceCtx->WaitForIdle();
    
    MapHelper<uint> counter_stage_map_data(m_pDeviceCtx, m_GetWorkQueueCS.CountStageBuffer, MAP_READ, MAP_FLAG_DO_NOT_WAIT);
    
    uint WorkQueueCount = 0;
    memcpy(&WorkQueueCount, counter_stage_map_data, sizeof(uint));

    if(WorkQueueCount > 0)
    {
        m_pDeviceCtx->SetPipelineState(m_DrawLineFromWorkQueueCS.PSO);

        m_DrawLineFromWorkQueueCS.SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutHairRenderTex")->Set( \
            pRTView->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));

        m_pDeviceCtx->CommitShaderResources(m_DrawLineFromWorkQueueCS.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
        const DispatchComputeAttribs attr(WorkQueueCount, 1, 1);
        m_pDeviceCtx->DispatchCompute(attr);
    }
}

void Diligent::HairRender::RunCS(const float4x4 &viwe_proj, const float4x4 &inv_view_proj, ITexture *pRTView)
{
    float2 PixelSize = float2(m_pSwapChain->GetDesc().Width, m_pSwapChain->GetDesc().Height);
    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<HairConstData> CBConstants(m_pDeviceCtx, m_HairConstData, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants->ViewProj = viwe_proj.Transpose();
        CBConstants->InvViewProj = inv_view_proj.Transpose();
        CBConstants->ScreenSize = PixelSize;
        CBConstants->DownSampleDepthSize = float2(m_DownSampledDepthSize.x, m_DownSampledDepthSize.y);
        CBConstants->HairBBoxMin = float4(m_HairRawData.HairBBoxMin, 1.0f);
        CBConstants->HairBBoxSize = float4(m_HairRawData.HairBBoxMax - m_HairRawData.HairBBoxMin, 1.0f);
    }

    RunDownSampledDepthMapCS();
    RunDrawLineCS();

    RunFrustumVoxelCullLineSizeCS();
    RunGetLineOffsetAndCounterCS();
    RunGetLineVisibilityCS();
    RunGetWorkQueueCS();
    RunDrawLineFromWorkQueueCS(pRTView);
}
