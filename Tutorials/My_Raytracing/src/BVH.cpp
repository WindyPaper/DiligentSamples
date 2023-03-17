#include "BVH.h"

#include <assert.h>

#include "Buffer.h"
#include "Texture.h"
#include "PipelineState.h"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "Shader.h"
#include "MapHelper.hpp"

Diligent::BVH::BVH(IDeviceContext *pDeviceCtx, IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory) :
	m_pDeviceCtx(pDeviceCtx),
	m_pDevice(pDevice),
	m_pShaderFactory(pShaderFactory)
{
	InitTestMesh();

	InitBuffer();

	InitPSO();
}

Diligent::BVH::~BVH()
{

}

void Diligent::BVH::InitTestMesh()
{
	BVHVertex CubeVerts[8] =
	{
		{float3(-1,-1,-1), float2(1,0)},
		{float3(-1,+1,-1), float2(0,1)},
		{float3(+1,+1,-1), float2(0,0)},
		{float3(+1,-1,-1), float2(1,1)},

		{float3(-1,-1,+1), float2(1,1)},
		{float3(-1,+1,+1), float2(0,1)},
		{float3(+1,+1,+1), float2(1,0)},
		{float3(+1,-1,+1), float2(1,1)},
	};

	//anti-clockwise
	Uint32 Indices[] =
	{
		2,0,1, 2,3,0,
		4,6,5, 4,7,6,
		0,7,4, 0,3,7,
		1,0,4, 1,4,5,
		1,5,2, 5,6,2,
		3,6,7, 3,2,6
	};

	// Create a vertex buffer that stores cube vertices
	BufferDesc VertBuffDesc;
	VertBuffDesc.Name = "Cube vertex buffer";
	VertBuffDesc.Usage = USAGE_IMMUTABLE;
	VertBuffDesc.BindFlags = BIND_SHADER_RESOURCE;
	VertBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	VertBuffDesc.ElementByteStride = sizeof(BVHVertex);
	VertBuffDesc.uiSizeInBytes = sizeof(CubeVerts);
	BufferData VBData;
	VBData.pData = CubeVerts;
	VBData.DataSize = sizeof(CubeVerts);
	m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_apMeshVertexData);

	BufferDesc IndBuffDesc;
	IndBuffDesc.Name = "Cube index buffer";
	IndBuffDesc.Usage = USAGE_IMMUTABLE;
	IndBuffDesc.BindFlags = BIND_SHADER_RESOURCE;
	IndBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	IndBuffDesc.ElementByteStride = sizeof(Uint32);
	IndBuffDesc.uiSizeInBytes = sizeof(Indices);
	BufferData IBData;
	IBData.pData = Indices;
	IBData.DataSize = sizeof(Indices);
	m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_apMeshIndexData);

	m_BVHMeshData.vertex_num = 8;
	m_BVHMeshData.index_num = 36;
	m_BVHMeshData.primitive_num = m_BVHMeshData.index_num / 3;

	Uint32 power_v = 1;
	while (power_v < m_BVHMeshData.primitive_num)
		power_v = power_v << 1;
	m_BVHMeshData.upper_pow_of_2_primitive_num = power_v;
}

void Diligent::BVH::InitBuffer()
{
	CreateGlobalBVHData();	
	CreatePrimAABBData(m_BVHMeshData.primitive_num);
	CreatePrimCenterMortonCodeData(m_BVHMeshData.upper_pow_of_2_primitive_num);
}

void Diligent::BVH::InitPSO()
{
	CreateGenerateAABBPSO();
	CreateReductionWholeAABBPSO();
	CreateGenerateMortonCodePSO();
}

void Diligent::BVH::BuildBVH()
{
	DispatchAABBBuild();
}

Diligent::RefCntAutoPtr<Diligent::IShader> Diligent::BVH::CreateShader(const std::string &entryPoint, const std::string &csFile, const std::string &descName, const SHADER_TYPE type, ShaderMacroHelper *pMacro)
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

Diligent::PipelineStateDesc Diligent::BVH::CreatePSODescAndParam(ShaderResourceVariableDesc *params, const int varNum, const std::string &psoName, const PIPELINE_TYPE type /*= PIPELINE_TYPE_COMPUTE*/)
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

void Diligent::BVH::CreatePrimCenterMortonCodeData(int num)
{
	BufferDesc MortonCodeBuffDesc;
	MortonCodeBuffDesc.Name = "Morton Code Buffer";
	MortonCodeBuffDesc.Usage = USAGE_DEFAULT;
	MortonCodeBuffDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	MortonCodeBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	MortonCodeBuffDesc.ElementByteStride = sizeof(Uint32);
	MortonCodeBuffDesc.uiSizeInBytes = sizeof(Uint32) * num;
	m_pDevice->CreateBuffer(MortonCodeBuffDesc, nullptr, &m_apPrimCenterMortonCodeData);
}

void Diligent::BVH::CreatePrimAABBData(int num)
{
	BufferDesc AABBBuffDesc;
	AABBBuffDesc.Name = "Prim AABB Buffer";
	AABBBuffDesc.Usage = USAGE_DEFAULT;
	AABBBuffDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	AABBBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	AABBBuffDesc.ElementByteStride = sizeof(BVHAABB);
	AABBBuffDesc.uiSizeInBytes = sizeof(BVHAABB) * (num * 2 - 1);
	m_pDevice->CreateBuffer(AABBBuffDesc, nullptr, &m_apPrimAABBData);

	BufferDesc WholeAABBBuffDesc;
	WholeAABBBuffDesc.Name = "Whole AABB Buffer Ping";
	WholeAABBBuffDesc.Usage = USAGE_DEFAULT;
	WholeAABBBuffDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	WholeAABBBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	WholeAABBBuffDesc.ElementByteStride = sizeof(BVHAABB);

	Uint32 PerGroupAABBListLength = ceilf(float(m_BVHMeshData.primitive_num) / ReductionGroupThreadNum);
	WholeAABBBuffDesc.uiSizeInBytes = sizeof(BVHAABB) * PerGroupAABBListLength;
	m_pDevice->CreateBuffer(WholeAABBBuffDesc, nullptr, &m_apWholeAABBDataPing);
	WholeAABBBuffDesc.Name = "Whole AABB Buffer Pong";
	m_pDevice->CreateBuffer(WholeAABBBuffDesc, nullptr, &m_apWholeAABBDataPong);

	BufferDesc ReductionAABBUniformBuffDesc;
	ReductionAABBUniformBuffDesc.Name = "Reduction AABB uniform buffer";
	ReductionAABBUniformBuffDesc.Usage = USAGE_DYNAMIC;
	ReductionAABBUniformBuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
	ReductionAABBUniformBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
	ReductionAABBUniformBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	ReductionAABBUniformBuffDesc.ElementByteStride = sizeof(ReductionUniformData);
	ReductionAABBUniformBuffDesc.uiSizeInBytes = sizeof(ReductionUniformData);
	m_pDevice->CreateBuffer(ReductionAABBUniformBuffDesc, nullptr, &m_apReductionUniformData);

}

void Diligent::BVH::CreateGlobalBVHData()
{
	BufferDesc GlobalBVHBuffDesc;
	GlobalBVHBuffDesc.Name = "global bvh data";
	GlobalBVHBuffDesc.Usage = USAGE_DYNAMIC;
	GlobalBVHBuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
	GlobalBVHBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
	GlobalBVHBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	GlobalBVHBuffDesc.ElementByteStride = sizeof(BVHGlobalData);
	GlobalBVHBuffDesc.uiSizeInBytes = sizeof(BVHGlobalData);
	m_pDevice->CreateBuffer(GlobalBVHBuffDesc, nullptr, &m_apGlobalBVHData);
}

void Diligent::BVH::CreateGenerateAABBPSO()
{
	RefCntAutoPtr<IShader> pGenerateAABBCS = CreateShader("GenerateAABBMain", "GenerateAABB.csh", "Generate AABB CS");

	ComputePipelineStateCreateInfo PSOCreateInfo;	

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "BVHGlobalData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "MeshVertex", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "MeshIdx", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "OutAABB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},		
	};
	// clang-format on
	PSOCreateInfo.PSODesc = CreatePSODescAndParam(Vars, _countof(Vars), "bvh generate AABB compute shader");	
	
	PSOCreateInfo.pCS = pGenerateAABBCS;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apGenAABBPSO);

	//SRB
	m_apGenAABBPSO->CreateShaderResourceBinding(&m_apGenAABBSRB, true);
}

void Diligent::BVH::CreateReductionWholeAABBPSO()
{
	ShaderMacroHelper Macros;
	Macros.AddShaderMacro("PER_GROUP_THREADS_NUM", ReductionGroupThreadNum);
	RefCntAutoPtr<IShader> pWholeAABBCS = CreateShader("ReductionWholeAABBMain", "ReductionWholeAABB.csh", "Reduction Whole AABB CS", SHADER_TYPE_COMPUTE, &Macros);

	ComputePipelineStateCreateInfo PSOCreateInfo;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "ReductionUniformData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},		
		{SHADER_TYPE_COMPUTE, "OutWholeAABB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
	};
	// clang-format on
	PSOCreateInfo.PSODesc = CreatePSODescAndParam(Vars, _countof(Vars), "bvh reduct whole AABB compute shader");

	PSOCreateInfo.pCS = pWholeAABBCS;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apWholeAABBPSO);

	//SRB
	m_apWholeAABBPSO->CreateShaderResourceBinding(&m_apWholeAABBSRB, true);
}

void Diligent::BVH::CreateGenerateMortonCodePSO()
{
	RefCntAutoPtr<IShader> pGenerateMortonCode = CreateShader("GeneratePrimMortonCodeMain", "GeneratePrimMortonCode.csh", "Generate prim morton code cs");

	ComputePipelineStateCreateInfo PSOCreateInfo;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "BVHGlobalData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "inAABB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "outPrimMortonCode", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
	};
	// clang-format on
	PSOCreateInfo.PSODesc = CreatePSODescAndParam(Vars, _countof(Vars), "bvh generate morton code pso");

	PSOCreateInfo.pCS = pGenerateMortonCode;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apGenMortonCodePSO);

	//SRB
	m_apGenMortonCodePSO->CreateShaderResourceBinding(&m_apGenMortonCodeSRB, true);
}

void Diligent::BVH::DispatchAABBBuild()
{
	//set pso
	m_pDeviceCtx->SetPipelineState(m_apGenAABBPSO);

	//bind data
	{
		MapHelper<BVHGlobalData> CBGlobalData(m_pDeviceCtx, m_apGlobalBVHData, MAP_WRITE, MAP_FLAG_DISCARD);
		CBGlobalData->num_objects = m_BVHMeshData.primitive_num;
		CBGlobalData->num_interal_nodes = CBGlobalData->num_objects - 1;
		CBGlobalData->num_all_nodes = 2 * CBGlobalData->num_objects - 1;
	}

	m_apGenAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "BVHGlobalData")->Set(m_apGlobalBVHData);
	m_apGenAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "MeshVertex")->Set(m_apMeshVertexData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
	m_apGenAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "MeshIdx")->Set(m_apMeshIndexData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
	m_apGenAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutAABB")->Set(m_apPrimAABBData->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

	//commit res data
	m_pDeviceCtx->CommitShaderResources(m_apGenAABBSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	//dispatch prim AABB
	const float sqrt_prim_num = std::sqrtf(m_BVHMeshData.primitive_num);
	//assert((prim_num & (prim_num - 1)) == 0);
	DispatchComputeAttribs attr(std::ceilf(sqrt_prim_num / 8.0f), std::ceilf(sqrt_prim_num / 8.0f));
	m_pDeviceCtx->DispatchCompute(attr);

	//Reduction AABB
	ReductionWholeAABB();
}

void Diligent::BVH::ReductionWholeAABB()
{
	m_pDeviceCtx->SetPipelineState(m_apWholeAABBPSO);

	Uint32 curr_grp_num;
	float deal_aabb_num = float(m_BVHMeshData.primitive_num);
	Uint32 whole_pass = 0;
	do 
	{	
		//set uniform value
		{
			MapHelper<ReductionUniformData> CBReductionData(m_pDeviceCtx, m_apReductionUniformData, MAP_WRITE, MAP_FLAG_DISCARD);
			CBReductionData->InReductionDataNum = deal_aabb_num;
		}
		m_apWholeAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "ReductionUniformData")->Set(m_apReductionUniformData);

		IBuffer *pInAABBBuf;
		IBuffer *pOutAABBBuf;
		if ((whole_pass & 1) == 0)
		{
			pInAABBBuf = m_apWholeAABBDataPing;
			pOutAABBBuf = m_apWholeAABBDataPong;
		}
		else
		{
			pInAABBBuf = m_apWholeAABBDataPong;
			pOutAABBBuf = m_apWholeAABBDataPing;
		}
		m_apWholeAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "InWholeAABB")->Set(pInAABBBuf->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
		m_apWholeAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutWholeAABB")->Set(pOutAABBBuf->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

		//commit res data
		m_pDeviceCtx->CommitShaderResources(m_apWholeAABBSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		curr_grp_num = ceilf(deal_aabb_num / ReductionGroupThreadNum);
		DispatchComputeAttribs attr(curr_grp_num, 1);
		m_pDeviceCtx->DispatchCompute(attr);

		deal_aabb_num = curr_grp_num;

	} while (curr_grp_num > 1);

	//Uint32 curr_grp_threads_num = ceilf(float(m_BVHMeshData.primitive_num) / ReductionGroupThreadNum);
	
}

