#include "BVHTrace.h"

#include "Buffer.h"
#include "Texture.h"
#include "PipelineState.h"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "Shader.h"
#include "MapHelper.hpp"
#include "BVH.h"


Diligent::BVHTrace::BVHTrace(IDeviceContext *pDeviceCtx, IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory, ISwapChain* pSwapChain, BVH *pBVH, const FirstPersonCamera &cam) :
	m_pDeviceCtx(pDeviceCtx),
	m_pDevice(pDevice),
	m_pShaderFactory(pShaderFactory),
	m_pSwapChain(pSwapChain),
	m_pBVH(pBVH),
	m_Camera(cam)
{
	CreateBuffer();
	CreateTracePSO();

	BindDiffTexs();
}

Diligent::BVHTrace::~BVHTrace()
{

}

void Diligent::BVHTrace::Update(const FirstPersonCamera &cam)
{
	m_Camera = cam;
}

void Diligent::BVHTrace::DispatchBVHTrace()
{
	m_pDeviceCtx->SetPipelineState(m_apTracePSO);

	float2 PixelSize = float2(m_pSwapChain->GetDesc().Width, m_pSwapChain->GetDesc().Height);

	{
		MapHelper<TraceUniformData> TraceCBData(m_pDeviceCtx, m_apTraceUniformData, MAP_WRITE, MAP_FLAG_DISCARD);
		float4x4 CamViewProjM = m_Camera.GetViewProjMatrix();
		TraceCBData->InvViewProjMatrix = CamViewProjM.Inverse();
		TraceCBData->CameraWPos = m_Camera.GetPos();
		TraceCBData->ScreenSize = PixelSize;
	}

	m_apTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "TraceUniformData")->Set(m_apTraceUniformData);
	IShaderResourceVariable* pMeshVertex = m_apTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "MeshVertex");
	if(pMeshVertex)
		pMeshVertex->Set(m_pBVH->GetMeshVertexBufferView());
	IShaderResourceVariable* pMeshIdx = m_apTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "MeshIdx");
	if(pMeshIdx)
		pMeshIdx->Set(m_pBVH->GetMeshIdxBufferView());
	m_apTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "BVHNodeData")->Set(m_pBVH->GetBVHNodeBufferView());
	m_apTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "BVHNodeAABB")->Set(m_pBVH->GetBVHNodeAABBBufferView());
	m_apTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutPixel")->Set(m_apOutRTPixelTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));

	m_pDeviceCtx->CommitShaderResources(m_apTraceSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	DispatchComputeAttribs attr(std::ceilf(PixelSize.x / 16.0f), std::ceilf(PixelSize.y / 16.0f));
	m_pDeviceCtx->DispatchCompute(attr);
}

Diligent::ITexture * Diligent::BVHTrace::GetOutputPixelTex()
{
	return m_apOutRTPixelTex;
}

Diligent::RefCntAutoPtr<Diligent::IShader> Diligent::BVHTrace::CreateShader(const std::string &entryPoint, const std::string &csFile, const std::string &descName, const SHADER_TYPE type /*= SHADER_TYPE_COMPUTE*/, ShaderMacroHelper *pMacro /*= nullptr*/)
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

Diligent::PipelineStateDesc Diligent::BVHTrace::CreatePSODescAndParam(ShaderResourceVariableDesc *params, const int varNum, const std::string &psoName, const PIPELINE_TYPE type /*= PIPELINE_TYPE_COMPUTE*/)
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

void Diligent::BVHTrace::CreateTracePSO()
{
	ShaderMacroHelper Macros;
	Macros.AddShaderMacro("DIFFUSE_TEX_NUM", m_pBVH->GetTextures()->size());
	RefCntAutoPtr<IShader> pTraceShader = CreateShader("TraceMain", "Trace.csh", "trace cs", SHADER_TYPE_COMPUTE, &Macros);

	ComputePipelineStateCreateInfo PSOCreateInfo;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "MeshVertex", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "MeshIdx", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "BVHNodeData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "BVHNodeAABB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "TraceUniformData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "DiffTextures", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
		{SHADER_TYPE_COMPUTE, "OutPixel", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
	};
	// clang-format on
	PSOCreateInfo.PSODesc = CreatePSODescAndParam(Vars, _countof(Vars), "trace pso");

	SamplerDesc SamLinearClampDesc
	{
		FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
		TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP
	};
	ImmutableSamplerDesc ImtblSamplers[] =
	{
		{SHADER_TYPE_COMPUTE, "DiffTextures", SamLinearClampDesc}
	};
	// clang-format on
	PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;
	PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

	PSOCreateInfo.pCS = pTraceShader;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apTracePSO);

	//SRB
	m_apTracePSO->CreateShaderResourceBinding(&m_apTraceSRB, true);
}

void Diligent::BVHTrace::CreateBuffer()
{
	BufferDesc TraceUniformBuffDesc;
	TraceUniformBuffDesc.Name = "trace uniform buffer";
	TraceUniformBuffDesc.Usage = USAGE_DYNAMIC;
	TraceUniformBuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
	TraceUniformBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
	TraceUniformBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	TraceUniformBuffDesc.ElementByteStride = sizeof(TraceUniformData);
	TraceUniformBuffDesc.uiSizeInBytes = sizeof(TraceUniformData);
	m_pDevice->CreateBuffer(TraceUniformBuffDesc, nullptr, &m_apTraceUniformData);

	TextureDesc TraceOutputTexDesc;
	TraceOutputTexDesc.Name = "trace output pixel data";
	TraceOutputTexDesc.Type = RESOURCE_DIM_TEX_2D;
	const auto& SCDesc = m_pSwapChain->GetDesc();
	TraceOutputTexDesc.Width = SCDesc.Width;
	TraceOutputTexDesc.Height = SCDesc.Height;
	TraceOutputTexDesc.MipLevels = 1;
	TraceOutputTexDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
	TraceOutputTexDesc.Usage = USAGE_DYNAMIC;
	TraceOutputTexDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	m_pDevice->CreateTexture(TraceOutputTexDesc, nullptr, &m_apOutRTPixelTex);
}

void Diligent::BVHTrace::BindDiffTexs()
{
	std::vector<RefCntAutoPtr<ITexture>> *pDiffTexArray = m_pBVH->GetTextures();
	std::vector<IDeviceObject*> pTexSRVs(pDiffTexArray->size());
	for (int i = 0; i < pDiffTexArray->size(); ++i)
	{
		pTexSRVs[i] = (*pDiffTexArray)[i]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
	}
	m_apTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "DiffTextures")->SetArray(&pTexSRVs[0], 0, pTexSRVs.size());	
}
