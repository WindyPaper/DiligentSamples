#include "BVHTrace.h"

#include "Buffer.h"
#include "Texture.h"
#include "PipelineState.h"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "Shader.h"
#include "MapHelper.hpp"
#include "BVH.h"

#include "assimp/Exporter.hpp"


Diligent::BVHTrace::BVHTrace(IDeviceContext *pDeviceCtx, IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory, ISwapChain* pSwapChain, BVH *pBVH, const FirstPersonCamera &cam, const std::string &mesh_file_name) :
	m_pDeviceCtx(pDeviceCtx),
	m_pDevice(pDevice),
	m_pShaderFactory(pShaderFactory),
	m_pSwapChain(pSwapChain),
	m_pBVH(pBVH),
	m_Camera(cam),
	m_mesh_file_name(mesh_file_name)
{
	CreateBuffer();
	CreateTracePSO();

	BindDiffTexs();

	CreateGenVertexAORaysPSO();
	CreateGenVertexAORaysBuffer();

	CreateVertexAOTracePSO();
	CreateVertexAOTraceBuffer();
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
	IShaderResourceVariable* pMeshPrimData = m_apTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "MeshPrimData");
	if (pMeshPrimData)
		pMeshPrimData->Set(m_pBVH->GetMeshPrimBufferView());
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

void Diligent::BVHTrace::DispatchVertexAOTrace()
{
	GenVertexAORays();

	m_pDeviceCtx->SetPipelineState(m_apVertexAOTracePSO);

	/*IShaderResourceVariable* pGenVertexAOUniformData = m_apVertexAOTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "GenVertexAORaysUniformData");
	if (pGenVertexAOUniformData)
	{
		{
			MapHelper<GenVertexAORaysUniformData> CBGlobalData(m_pDeviceCtx, m_apVertexAORaysUniformBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
			CBGlobalData->num_vertex = m_pBVH->GetBVHMeshData().vertex_num;
		}
		pGenVertexAOUniformData->Set(m_apVertexAORaysUniformBuffer);
	}*/

	IShaderResourceVariable* pMeshIdx = m_apVertexAOTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "MeshIdx");
	if (pMeshIdx)
		pMeshIdx->Set(m_pBVH->GetMeshIdxBufferView());
	m_apVertexAOTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "MeshVertex")->Set(m_pBVH->GetMeshVertexBufferView());
	m_apVertexAOTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "BVHNodeData")->Set(m_pBVH->GetBVHNodeBufferView());
	m_apVertexAOTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "BVHNodeAABB")->Set(m_pBVH->GetBVHNodeAABBBufferView());
	m_apVertexAOTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "BakeAOTexture")->Set(m_pBVH->GetAOTexture()->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	m_apVertexAOTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "AORayDatas")->Set(m_apVertexAOOutRaysBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
	m_apVertexAOTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutAOColorDatas")->Set(m_apVertexAOColorBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

	m_pDeviceCtx->CommitShaderResources(m_apVertexAOTraceSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	DispatchComputeAttribs attr(m_pBVH->GetBVHMeshData().vertex_num, 1);
	m_pDeviceCtx->DispatchCompute(attr);

	//read back color buffer to CPU
	m_pDeviceCtx->CopyBuffer(m_apVertexAOColorBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
		m_apVertexAOColorStageBuffer, 0, sizeof(GenAOColorData) * m_pBVH->GetBVHMeshData().vertex_num,
		RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	//sync gpu finish copy operation.
	m_pDeviceCtx->WaitForIdle();

	MapHelper<GenAOColorData> map_ao_color_datas(m_pDeviceCtx, m_apVertexAOColorStageBuffer, MAP_READ, MAP_FLAG_DO_NOT_WAIT);

	std::vector<GenAOColorData> out_color_cpu;
	out_color_cpu.resize(m_pBVH->GetBVHMeshData().vertex_num);
	memcpy(&out_color_cpu[0], &(*map_ao_color_datas), sizeof(GenAOColorData) * out_color_cpu.size());

	aiScene *pFBXScene = m_pBVH->GetAssimpScene();
	unsigned int mesh_num = pFBXScene->mNumMeshes;
	Uint32 vertex_idx = 0;
	for (unsigned int mesh_i = 0; mesh_i < mesh_num; ++mesh_i)
	{
		aiMesh* mesh_ptr = pFBXScene->mMeshes[mesh_i];

		int vertex_num = mesh_ptr->mNumVertices;

		for (int i = 0; i < vertex_num; ++i)
		{			
			//set vertex color
			aiColor4D** t_vertex_color = &(mesh_ptr->mColors[0]);
			if (*t_vertex_color == nullptr)
			{
				*t_vertex_color = new aiColor4D[vertex_num];
			}
			mesh_ptr->mColors[0][i].r = out_color_cpu[vertex_idx++].lum;
			mesh_ptr->mColors[0][i].g = 0.0f;
			mesh_ptr->mColors[0][i].b = 0.0f;
			mesh_ptr->mColors[0][i].a = 0.0f;
		}
	}

	Assimp::Exporter exp;
	std::string out_put_mesh_name = m_mesh_file_name.substr(0, m_mesh_file_name.find('.')) + "_vc" + m_mesh_file_name.substr(m_mesh_file_name.find('.'));
	exp.Export(pFBXScene, "fbxa", out_put_mesh_name.c_str());

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

void Diligent::BVHTrace::CreateGenVertexAORaysPSO()
{	
	ShaderMacroHelper Macros;
	Macros.AddShaderMacro("VERTEX_AO_SAMPLE_NUM", VERTEX_AO_RAY_SAMPLE_NUM);
	RefCntAutoPtr<IShader> pGenVertexAORaysShader = CreateShader("GenVertexAORaysMain", "Trace.csh", "gen vertex ao rays cs", SHADER_TYPE_COMPUTE, &Macros);

	ComputePipelineStateCreateInfo PSOCreateInfo;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "GenVertexAORaysUniformData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "MeshVertex", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "OutAORayDatas", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},		
	};
	// clang-format on
	PSOCreateInfo.PSODesc = CreatePSODescAndParam(Vars, _countof(Vars), "gen vertex ao rays pso");	

	PSOCreateInfo.pCS = pGenVertexAORaysShader;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apGenVertexAORaysPSO);

	//SRB
	m_apGenVertexAORaysPSO->CreateShaderResourceBinding(&m_apGenVertexAORaysSRB, true);
}

void Diligent::BVHTrace::CreateGenVertexAORaysBuffer()
{
	BufferDesc GenVertexAOUniformDataDesc;
	GenVertexAOUniformDataDesc.Name = "Gen Vertex AO Uniform Data Desc";
	GenVertexAOUniformDataDesc.Usage = USAGE_DYNAMIC;
	GenVertexAOUniformDataDesc.BindFlags = BIND_UNIFORM_BUFFER;
	GenVertexAOUniformDataDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
	GenVertexAOUniformDataDesc.Mode = BUFFER_MODE_STRUCTURED;
	GenVertexAOUniformDataDesc.ElementByteStride = sizeof(GenVertexAORaysUniformData);
	GenVertexAOUniformDataDesc.uiSizeInBytes = sizeof(GenVertexAORaysUniformData);	
	m_pDevice->CreateBuffer(GenVertexAOUniformDataDesc, nullptr, &m_apVertexAORaysUniformBuffer);

	BufferDesc VertexAOOutRayBuffDesc;
	VertexAOOutRayBuffDesc.Name = "vertex ao out ray datas";
	VertexAOOutRayBuffDesc.Usage = USAGE_DEFAULT;
	VertexAOOutRayBuffDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	VertexAOOutRayBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	VertexAOOutRayBuffDesc.ElementByteStride = sizeof(GenAORayData);
	VertexAOOutRayBuffDesc.uiSizeInBytes = sizeof(GenAORayData) * m_pBVH->GetBVHMeshData().vertex_num * VERTEX_AO_RAY_SAMPLE_NUM;
	m_pDevice->CreateBuffer(VertexAOOutRayBuffDesc, nullptr, &m_apVertexAOOutRaysBuffer);
}

void Diligent::BVHTrace::GenVertexAORays()
{
	m_pDeviceCtx->SetPipelineState(m_apGenVertexAORaysPSO);

	IShaderResourceVariable* pGenVertexAOUniformData = m_apGenVertexAORaysSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "GenVertexAORaysUniformData");
	if(pGenVertexAOUniformData)
	{
		{
			MapHelper<GenVertexAORaysUniformData> CBGlobalData(m_pDeviceCtx, m_apVertexAORaysUniformBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
			CBGlobalData->num_vertex = m_pBVH->GetBVHMeshData().vertex_num;
		}
		pGenVertexAOUniformData->Set(m_apVertexAORaysUniformBuffer);
	}
	
	m_apGenVertexAORaysSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "MeshVertex")->Set(m_pBVH->GetMeshVertexBufferView());
	m_apGenVertexAORaysSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutAORayDatas")->Set(m_apVertexAOOutRaysBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

	m_pDeviceCtx->CommitShaderResources(m_apGenVertexAORaysSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	DispatchComputeAttribs attr(m_pBVH->GetBVHMeshData().vertex_num, 1);
	m_pDeviceCtx->DispatchCompute(attr);
}

void Diligent::BVHTrace::CreateVertexAOTracePSO()
{
	ShaderMacroHelper Macros;
	Macros.AddShaderMacro("VERTEX_AO_SAMPLE_NUM", VERTEX_AO_RAY_SAMPLE_NUM);
	RefCntAutoPtr<IShader> pGenVertexAOTraceShader = CreateShader("GenVertexAOColorMain", "Trace.csh", "gen vertex ao color cs", SHADER_TYPE_COMPUTE, &Macros);

	ComputePipelineStateCreateInfo PSOCreateInfo;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{		
		{SHADER_TYPE_COMPUTE, "MeshIdx", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "BVHNodeData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "BVHNodeAABB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "MeshVertex", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "AORayDatas", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "BakeAOTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},		
		{SHADER_TYPE_COMPUTE, "OutAOColorDatas", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
	};

	// clang-format on
	PSOCreateInfo.PSODesc = CreatePSODescAndParam(Vars, _countof(Vars), "gen vertex ao color pso");

	SamplerDesc SamLinearClampDesc
	{
		FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
		TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP
	};
	ImmutableSamplerDesc ImtblSamplers[] =
	{
		{SHADER_TYPE_COMPUTE, "BakeAOTexture", SamLinearClampDesc}
	};
	// clang-format on
	PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;
	PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

	PSOCreateInfo.pCS = pGenVertexAOTraceShader;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apVertexAOTracePSO);

	//SRB
	m_apVertexAOTracePSO->CreateShaderResourceBinding(&m_apVertexAOTraceSRB, true);
}

void Diligent::BVHTrace::CreateVertexAOTraceBuffer()
{
	BufferDesc VertexAOOutColorBuffDesc;
	VertexAOOutColorBuffDesc.Name = "vertex ao out color datas";
	VertexAOOutColorBuffDesc.Usage = USAGE_DEFAULT;
	VertexAOOutColorBuffDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	VertexAOOutColorBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	VertexAOOutColorBuffDesc.ElementByteStride = sizeof(GenAOColorData);
	VertexAOOutColorBuffDesc.uiSizeInBytes = sizeof(GenAOColorData) * m_pBVH->GetBVHMeshData().vertex_num;
	m_pDevice->CreateBuffer(VertexAOOutColorBuffDesc, nullptr, &m_apVertexAOColorBuffer);

	BufferDesc VertexAOOutColorStageBuffer;
	VertexAOOutColorStageBuffer.Name = "gen vertex ao color staging buffer";
	VertexAOOutColorStageBuffer.Usage = USAGE_STAGING;
	VertexAOOutColorStageBuffer.BindFlags = BIND_NONE;
	VertexAOOutColorStageBuffer.Mode = BUFFER_MODE_UNDEFINED;
	VertexAOOutColorStageBuffer.CPUAccessFlags = CPU_ACCESS_READ;
	VertexAOOutColorStageBuffer.uiSizeInBytes = VertexAOOutColorBuffDesc.uiSizeInBytes;
	VertexAOOutColorStageBuffer.ElementByteStride = VertexAOOutColorBuffDesc.ElementByteStride;
	m_pDevice->CreateBuffer(VertexAOOutColorStageBuffer, nullptr, &m_apVertexAOColorStageBuffer);
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
		{SHADER_TYPE_COMPUTE, "MeshPrimData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
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
