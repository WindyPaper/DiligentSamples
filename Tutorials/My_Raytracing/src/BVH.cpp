#include "BVH.h"

#include <assert.h>
#include <vector>
#include <numeric>

#include "Buffer.h"
#include "Texture.h"
#include "PipelineState.h"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "Shader.h"
#include "MapHelper.hpp"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

Diligent::BVH::BVH(IDeviceContext *pDeviceCtx, IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory) :
	m_pDeviceCtx(pDeviceCtx),
	m_pDevice(pDevice),
	m_pShaderFactory(pShaderFactory),
	m_pOutWholeAABB(nullptr),
	m_pOutResultSortData(nullptr)
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
	//BVHVertex CubeVerts[8] =
	//{
	//	{float3(-11,-1,-1), float2(1,0)},
	//	{float3(-1,+1,-1), float2(0,1)},
	//	{float3(+1,+1,-1), float2(0,0)},
	//	{float3(+1,-1,-1), float2(1,1)},

	//	{float3(-1,-1,+1), float2(1,1)},
	//	{float3(-1,+1,+1), float2(0,1)},
	//	{float3(+1,+1,+1), float2(1,0)},
	//	{float3(+12,-1,+1), float2(1,1)},
	//};

	////anti-clockwise
	//Uint32 Indices[] =
	//{
	//	2,0,1, 2,3,0,
	//	4,6,5, 4,7,6,
	//	0,7,4, 0,3,7,
	//	1,0,4, 1,4,5,
	//	1,5,2, 5,6,2,
	//	3,6,7, 3,2,6
	//};

	//// Create a vertex buffer that stores cube vertices
	//BufferDesc VertBuffDesc;
	//VertBuffDesc.Name = "Cube vertex buffer";
	//VertBuffDesc.Usage = USAGE_IMMUTABLE;
	//VertBuffDesc.BindFlags = BIND_SHADER_RESOURCE;
	//VertBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	//VertBuffDesc.ElementByteStride = sizeof(BVHVertex);
	//VertBuffDesc.uiSizeInBytes = sizeof(CubeVerts);
	//BufferData VBData;
	//VBData.pData = CubeVerts;
	//VBData.DataSize = sizeof(CubeVerts);
	//m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_apMeshVertexData);

	//BufferDesc IndBuffDesc;
	//IndBuffDesc.Name = "Cube index buffer";
	//IndBuffDesc.Usage = USAGE_IMMUTABLE;
	//IndBuffDesc.BindFlags = BIND_SHADER_RESOURCE;
	//IndBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	//IndBuffDesc.ElementByteStride = sizeof(Uint32);
	//IndBuffDesc.uiSizeInBytes = sizeof(Indices);
	//BufferData IBData;
	//IBData.pData = Indices;
	//IBData.DataSize = sizeof(Indices);
	//m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_apMeshIndexData);

	//m_BVHMeshData.vertex_num = 8;
	//m_BVHMeshData.index_num = 36;
	//m_BVHMeshData.primitive_num = m_BVHMeshData.index_num / 3;

	//Uint32 power_v = 1;
	//while (power_v < m_BVHMeshData.primitive_num)
	//	power_v = power_v << 1;
	//m_BVHMeshData.upper_pow_of_2_primitive_num = power_v;

	LoadFBXFile("test_sphere.FBX");
}

void Diligent::BVH::LoadFBXFile(const std::string &name)
{
	

	int indices_offset = 0;

	std::vector<BVHVertex> mesh_vertex_data;
	std::vector<Uint32> mesh_index_data;

	using namespace Assimp;
	Importer importer;
	unsigned int flags = aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_PreTransformVertices |
		aiProcess_RemoveRedundantMaterials |
		aiProcess_OptimizeMeshes |
		aiProcess_ConvertToLeftHanded;
	const aiScene* import_fbx_scene = importer.ReadFile(name, flags);

	if (import_fbx_scene == NULL)
	{
		std::string error_code = importer.GetErrorString();
		printf("Load FBX", "load fbx file failed! " + error_code);
		return;
	}

	unsigned int mesh_num = import_fbx_scene->mNumMeshes;
	//unsigned int mat_num = import_fbx_scene->mNumMaterials;


	//Mesh* pMesh = new Mesh();

	for (unsigned int mesh_i = 0; mesh_i < mesh_num; ++mesh_i)
	{
		aiMesh* mesh_ptr = import_fbx_scene->mMeshes[mesh_i];

		//Mesh* p_cy_mesh = fbx_add_mesh(scene, transform_identity());
		//p_cy_mesh->reserve_mesh(vertex_num, triangle_num);		

		int vertex_num = mesh_ptr->mNumVertices;

		for (int i = 0; i < vertex_num; ++i)
		{
			const aiVector3D& v = mesh_ptr->mVertices[i];
			const aiVector3D& uv = mesh_ptr->mTextureCoords[0][i];
			//const aiVector3D& normal = mesh_ptr->mNormals[i];

			mesh_vertex_data.emplace_back(BVHVertex(float3(v.x, v.y, v.z), float2(uv.x, uv.y)));			
		}

		int triangle_num = mesh_ptr->mNumFaces;
		//int index_num = triangle_num * 3;
		for (int i = 0; i < triangle_num; ++i)
		{
			const aiFace& face = mesh_ptr->mFaces[i];
			//pMesh->triangles.emplace_back(Triangle{ face.mIndices[0], face.mIndices[1], face.mIndices[2] });
			for (int tri = 0; tri < 3; ++tri)
			{
				mesh_index_data.emplace_back(face.mIndices[tri] + indices_offset);
			}
		}
		indices_offset += triangle_num * 3;
	}
	importer.FreeScene();

	//Uint32 *p = &mesh_index_data[0];

	// Create a vertex buffer that stores cube vertices
	BufferDesc VertBuffDesc;
	VertBuffDesc.Name = "mesh vertex buffer";
	VertBuffDesc.Usage = USAGE_IMMUTABLE;
	VertBuffDesc.BindFlags = BIND_SHADER_RESOURCE;
	VertBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	VertBuffDesc.ElementByteStride = sizeof(BVHVertex);
	VertBuffDesc.uiSizeInBytes = sizeof(BVHVertex) * mesh_vertex_data.size();
	BufferData VBData;
	VBData.pData = &mesh_vertex_data[0];
	VBData.DataSize = VertBuffDesc.uiSizeInBytes;
	m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_apMeshVertexData);

	BufferDesc IndBuffDesc;
	IndBuffDesc.Name = "mesh index buffer";
	IndBuffDesc.Usage = USAGE_IMMUTABLE;
	IndBuffDesc.BindFlags = BIND_SHADER_RESOURCE;
	IndBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	IndBuffDesc.ElementByteStride = sizeof(Uint32);
	IndBuffDesc.uiSizeInBytes = sizeof(Uint32) * mesh_index_data.size();
	BufferData IBData;
	IBData.pData = &mesh_index_data[0];
	IBData.DataSize = IndBuffDesc.uiSizeInBytes;
	m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_apMeshIndexData);

	m_BVHMeshData.vertex_num = mesh_vertex_data.size();
	m_BVHMeshData.index_num = mesh_index_data.size();
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
	CreateSortMortonCodeData(m_BVHMeshData.upper_pow_of_2_primitive_num);
	CreateConstructBVHData(m_BVHMeshData.primitive_num * 2 - 1);
	CreateGenerateInternalAABBData(m_BVHMeshData.primitive_num - 1);
	CreateMergeBitonicSortData();

#if DILIGENT_DEBUG
	CreateDebugData();
#endif
}

void Diligent::BVH::InitPSO()
{
	CreateGenerateAABBPSO();
	CreateReductionWholeAABBPSO();
	CreateGenerateMortonCodePSO();
	CreateSortMortonCodePSO();
	CreateConstructBVHPSO();
	CreateMergeBitonicSortMortonCodePSO();

#if DILIGENT_DEBUG
	CreateDebugPSO();
#endif
}

void Diligent::BVH::BuildBVH()
{
	DispatchAABBBuild();
	DispatchMortonCodeBuild();
	DispatchSortMortonCode();
	DispatchMergeBitonicSortMortonCode();

#if DILIGENT_DEBUG
	DispatchDebugBVH();
#endif

	DispatchInitBVHNode();
	DispatchConstructBVHInternalNode();
	DispatchGenerateInternalNodeAABB();
}

Diligent::IBufferView* Diligent::BVH::GetMeshVertexBufferView()
{
	return m_apMeshVertexData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
}

Diligent::IBufferView* Diligent::BVH::GetMeshIdxBufferView()
{
	return m_apMeshIndexData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
}

Diligent::IBufferView* Diligent::BVH::GetBVHNodeBufferView()
{
	return m_apBVHNodeData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
}

Diligent::IBufferView* Diligent::BVH::GetBVHNodeAABBBufferView()
{	
	return m_apPrimAABBData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
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

	BufferDesc PrimCentroidBuffDesc;
	PrimCentroidBuffDesc.Name = "Prim Centroid Buffer";
	PrimCentroidBuffDesc.Usage = USAGE_DEFAULT;
	PrimCentroidBuffDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	PrimCentroidBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	PrimCentroidBuffDesc.ElementByteStride = sizeof(float3);
	PrimCentroidBuffDesc.uiSizeInBytes = sizeof(float3) * num;
	m_pDevice->CreateBuffer(PrimCentroidBuffDesc, nullptr, &m_apPrimCentroidData);	

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

void Diligent::BVH::CreateSortMortonCodeData(int num)
{
	BufferDesc SortMortonUniformBuffDesc;
	SortMortonUniformBuffDesc.Name = "SortMortonUniform Buff";
	SortMortonUniformBuffDesc.Usage = USAGE_DYNAMIC;
	SortMortonUniformBuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
	SortMortonUniformBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
	SortMortonUniformBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	SortMortonUniformBuffDesc.ElementByteStride = sizeof(SortMortonUniformData);
	SortMortonUniformBuffDesc.uiSizeInBytes = sizeof(SortMortonUniformData);
	m_pDevice->CreateBuffer(SortMortonUniformBuffDesc, nullptr, &m_apSortMortonCodeUniform);

	BufferDesc MortonCodeBuffDesc;
	MortonCodeBuffDesc.Name = "Sort Morton Code Buffer";
	MortonCodeBuffDesc.Usage = USAGE_DEFAULT;
	MortonCodeBuffDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	MortonCodeBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	MortonCodeBuffDesc.ElementByteStride = sizeof(Uint32);
	MortonCodeBuffDesc.uiSizeInBytes = sizeof(Uint32) * num;
	m_pDevice->CreateBuffer(MortonCodeBuffDesc, nullptr, &m_apOutSortMortonCodeData);

	BufferDesc IdxBuffDesc;
	IdxBuffDesc.Name = "Sort Idx Buffer Ping";
	IdxBuffDesc.Usage = USAGE_DEFAULT;
	IdxBuffDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	IdxBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	IdxBuffDesc.ElementByteStride = sizeof(Uint32);
	IdxBuffDesc.uiSizeInBytes = sizeof(Uint32) * num;

	std::vector<Uint32> idx_data(num);
	std::iota(idx_data.begin(), idx_data.end(), 0);
	BufferData init_buff_data;
	init_buff_data.DataSize = IdxBuffDesc.uiSizeInBytes;
	init_buff_data.pData = &idx_data[0];

	m_pDevice->CreateBuffer(IdxBuffDesc, &init_buff_data, &m_apOutSortIdxDataPing);
	IdxBuffDesc.Name = "Sort Idx Buffer Pong";
	m_pDevice->CreateBuffer(IdxBuffDesc, nullptr, &m_apOutSortIdxDataPong);
}

void Diligent::BVH::CreateConstructBVHData(int num_node)
{
	BufferDesc BVHNodeDesc;
	BVHNodeDesc.Name = "BVH Node Buffer";
	BVHNodeDesc.Usage = USAGE_DEFAULT;
	BVHNodeDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	BVHNodeDesc.Mode = BUFFER_MODE_STRUCTURED;
	BVHNodeDesc.ElementByteStride = sizeof(BVHNode);
	BVHNodeDesc.uiSizeInBytes = sizeof(BVHNode) * num_node;
	m_pDevice->CreateBuffer(BVHNodeDesc, nullptr, &m_apBVHNodeData);
}

void Diligent::BVH::CreateGenerateInternalAABBData(int num_internal_node)
{
	BufferDesc InternalNodeAABBFlagBuffDesc;
	InternalNodeAABBFlagBuffDesc.Name = "generate internal aabb flag data";
	InternalNodeAABBFlagBuffDesc.Usage = USAGE_DEFAULT;
	InternalNodeAABBFlagBuffDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	InternalNodeAABBFlagBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	InternalNodeAABBFlagBuffDesc.ElementByteStride = sizeof(Uint32);
	InternalNodeAABBFlagBuffDesc.uiSizeInBytes = sizeof(Uint32) * num_internal_node;

	std::vector<Uint32> flag_data(num_internal_node, 0);
	//std::iota(flag_data.begin(), flag_data.end(), 0);
	BufferData init_buff_data;
	init_buff_data.DataSize = InternalNodeAABBFlagBuffDesc.uiSizeInBytes;
	init_buff_data.pData = &flag_data[0];

	m_pDevice->CreateBuffer(InternalNodeAABBFlagBuffDesc, &init_buff_data, &m_apGenerateInternalNodeFlagData);
}

void Diligent::BVH::CreateMergeBitonicSortData()
{
	BufferDesc MergeBitonicSortMortonUniformBuffDesc;
	MergeBitonicSortMortonUniformBuffDesc.Name = "Merge Bitonic SortMortonUniform Buff";
	MergeBitonicSortMortonUniformBuffDesc.Usage = USAGE_DYNAMIC;
	MergeBitonicSortMortonUniformBuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
	MergeBitonicSortMortonUniformBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
	MergeBitonicSortMortonUniformBuffDesc.Mode = BUFFER_MODE_STRUCTURED;
	MergeBitonicSortMortonUniformBuffDesc.ElementByteStride = sizeof(MergeBitonicSortMortonUniformData);
	MergeBitonicSortMortonUniformBuffDesc.uiSizeInBytes = sizeof(MergeBitonicSortMortonUniformData);
	m_pDevice->CreateBuffer(MergeBitonicSortMortonUniformBuffDesc, nullptr, &m_apMergeBitonicSortUniformData);
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
		{SHADER_TYPE_COMPUTE, "OutPrimCentroid", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},		
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
		{SHADER_TYPE_COMPUTE, "whole_aabb", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "inAABBs", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "inPrimCentroid", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "outPrimMortonCode", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
	};
	// clang-format on
	PSOCreateInfo.PSODesc = CreatePSODescAndParam(Vars, _countof(Vars), "bvh generate morton code pso");

	PSOCreateInfo.pCS = pGenerateMortonCode;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apGenMortonCodePSO);

	//SRB
	m_apGenMortonCodePSO->CreateShaderResourceBinding(&m_apGenMortonCodeSRB, true);
}

void Diligent::BVH::CreateSortMortonCodePSO()
{
	ShaderMacroHelper Macros;
	Macros.AddShaderMacro("SORT_GROUP_THREADS_NUM", SortMortonCodeThreadNum);
	RefCntAutoPtr<IShader> pSortMortonCode = CreateShader("SortMortonCodeMain", "SortMortonCode.csh", "sort morton code cs", SHADER_TYPE_COMPUTE, &Macros);

	ComputePipelineStateCreateInfo PSOCreateInfo;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "BVHGlobalData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "SortMortonUniformData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "InMortonCodeData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "OutSortMortonCodeData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},		
		{SHADER_TYPE_COMPUTE, "OutIdxData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
	};
	// clang-format on
	PSOCreateInfo.PSODesc = CreatePSODescAndParam(Vars, _countof(Vars), "sort morton code pso");

	PSOCreateInfo.pCS = pSortMortonCode;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apSortMortonCodePSO);

	//SRB
	m_apSortMortonCodePSO->CreateShaderResourceBinding(&m_apSortMortonCodeSRB, true);
}

void Diligent::BVH::CreateMergeBitonicSortMortonCodePSO()
{	
	ShaderMacroHelper Macros;
	Macros.AddShaderMacro("SORT_GROUP_THREADS_NUM", SortMortonCodeThreadNum);
	RefCntAutoPtr<IShader> pMergeBitonicSortMortonCode = CreateShader("MergeBitonicSortMortonCodeMain", "MergeBitonicSortMortonCode.csh", "merge bitonic sort morton code cs", SHADER_TYPE_COMPUTE, &Macros);

	ComputePipelineStateCreateInfo PSOCreateInfo;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		//{SHADER_TYPE_COMPUTE, "BVHGlobalData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "MergeBitonicSortMortonUniformData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "InMortonCodeData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		//{SHADER_TYPE_COMPUTE, "InIdxData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "OutSortMortonCodeData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "OutIdxData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
	};
	// clang-format on
	PSOCreateInfo.PSODesc = CreatePSODescAndParam(Vars, _countof(Vars), "merge bitonic sort morton code pso");

	PSOCreateInfo.pCS = pMergeBitonicSortMortonCode;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apMergeBitonicSortPSO);

	//SRB
	m_apMergeBitonicSortPSO->CreateShaderResourceBinding(&m_apMergeBitonicSortSRB, true);
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
		CBGlobalData->upper_pow_of_2_primitive_num = m_BVHMeshData.upper_pow_of_2_primitive_num;
	}

	m_apGenAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "BVHGlobalData")->Set(m_apGlobalBVHData);
	m_apGenAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "MeshVertex")->Set(m_apMeshVertexData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
	m_apGenAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "MeshIdx")->Set(m_apMeshIndexData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
	m_apGenAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutAABB")->Set(m_apPrimAABBData->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
	m_apGenAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutPrimCentroid")->Set(m_apPrimCentroidData->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));	

	//commit res data
	m_pDeviceCtx->CommitShaderResources(m_apGenAABBSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	//dispatch prim AABB
	//const float sqrt_prim_num = std::sqrtf(m_BVHMeshData.primitive_num);
	//assert((prim_num & (prim_num - 1)) == 0);
	DispatchComputeAttribs attr(std::ceilf(m_BVHMeshData.primitive_num / 64.0f), 1);
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

			if (whole_pass == 0)
			{
				CBReductionData->InAABBIdxOffset = m_BVHMeshData.primitive_num - 1;
			}
			else
			{
				CBReductionData->InAABBIdxOffset = 0;
			}			
		}
		m_apWholeAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "ReductionUniformData")->Set(m_apReductionUniformData);

		IBuffer *pInAABBBuf;
		IBuffer *pOutAABBBuf;

		if (whole_pass == 0)
		{
			pInAABBBuf = m_apPrimAABBData;
			pOutAABBBuf = m_apWholeAABBDataPing;
		}
		else
		{
			if ((whole_pass & 1))
			{
				pInAABBBuf = m_apWholeAABBDataPing;
				pOutAABBBuf = m_apWholeAABBDataPong;
			}
			else
			{
				pInAABBBuf = m_apWholeAABBDataPong;
				pOutAABBBuf = m_apWholeAABBDataPing;
			}
		}
		m_apWholeAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "InWholeAABB")->Set(pInAABBBuf->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
		m_apWholeAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutWholeAABB")->Set(pOutAABBBuf->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

		//commit res data
		m_pDeviceCtx->CommitShaderResources(m_apWholeAABBSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		curr_grp_num = ceilf(deal_aabb_num / ReductionGroupThreadNum);
		DispatchComputeAttribs attr(curr_grp_num, 1);
		m_pDeviceCtx->DispatchCompute(attr);

		m_pOutWholeAABB = pOutAABBBuf;
		deal_aabb_num = curr_grp_num;
		++whole_pass;

	} while (curr_grp_num > 1);

	//Uint32 curr_grp_threads_num = ceilf(float(m_BVHMeshData.primitive_num) / ReductionGroupThreadNum);
	
}

void Diligent::BVH::DispatchMortonCodeBuild()
{
	m_pDeviceCtx->SetPipelineState(m_apGenMortonCodePSO);

	m_apGenMortonCodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "BVHGlobalData")->Set(m_apGlobalBVHData);
	m_apGenMortonCodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "whole_aabb")->Set(m_pOutWholeAABB->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
	//m_apGenMortonCodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "inAABBs")->Set(m_apPrimAABBData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
	m_apGenMortonCodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "inPrimCentroid")->Set(m_apPrimCentroidData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));	
	m_apGenMortonCodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "outPrimMortonCode")->Set(m_apPrimCenterMortonCodeData->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

	m_pDeviceCtx->CommitShaderResources(m_apGenMortonCodeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	DispatchComputeAttribs attr(std::ceilf(m_BVHMeshData.upper_pow_of_2_primitive_num / 64.0f), 1);
	m_pDeviceCtx->DispatchCompute(attr);
}

void Diligent::BVH::DispatchSortMortonCode()
{
	m_pDeviceCtx->SetPipelineState(m_apSortMortonCodePSO);

	Uint32 divide_group_num = ceilf((float)m_BVHMeshData.upper_pow_of_2_primitive_num / SortMortonCodeThreadNum);
	Uint32 pass_num = 30; //max morton code bit num

	for (Uint32 i = 0; i < pass_num; ++i)
	{
		{
			MapHelper<SortMortonUniformData> CBSortMortonCodeData(m_pDeviceCtx, m_apSortMortonCodeUniform, MAP_WRITE, MAP_FLAG_DISCARD);
			CBSortMortonCodeData->pass_id = i;
			//if (grp_idx == divide_group_num - 1)
			//{
			CBSortMortonCodeData->sort_num = std::fminf(m_BVHMeshData.upper_pow_of_2_primitive_num, SortMortonCodeThreadNum);
			//}
			/*else
			{
				CBSortMortonCodeData->sort_num = SortMortonCodeThreadNum;
			}*/
		}

		m_apSortMortonCodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "BVHGlobalData")->Set(m_apGlobalBVHData);
		m_apSortMortonCodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "SortMortonUniformData")->Set(m_apSortMortonCodeUniform);
		
		IBuffer *pInSortMortonCodeData;
		IBuffer *pOutSortMortonCodeData;
		IBuffer *pInSortIdxData;
		IBuffer *pOutSortIdxData;
		if ((i & 1) == 0)
		{
			pInSortMortonCodeData = m_apPrimCenterMortonCodeData;
			pOutSortMortonCodeData = m_apOutSortMortonCodeData;

			pInSortIdxData = m_apOutSortIdxDataPing;
			pOutSortIdxData = m_apOutSortIdxDataPong;
		}
		else
		{
			pInSortMortonCodeData = m_apOutSortMortonCodeData;
			pOutSortMortonCodeData = m_apPrimCenterMortonCodeData;

			pInSortIdxData = m_apOutSortIdxDataPong;
			pOutSortIdxData = m_apOutSortIdxDataPing;
		}

		m_apSortMortonCodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "InMortonCodeData")->Set(pInSortMortonCodeData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
		m_apSortMortonCodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutSortMortonCodeData")->Set(pOutSortMortonCodeData->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

		m_apSortMortonCodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "InIdxData")->Set(pInSortIdxData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
		m_apSortMortonCodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutIdxData")->Set(pOutSortIdxData->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

		m_pDeviceCtx->CommitShaderResources(m_apSortMortonCodeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		DispatchComputeAttribs attr(std::ceilf((float)m_BVHMeshData.primitive_num / SortMortonCodeThreadNum), 1);
		m_pDeviceCtx->DispatchCompute(attr);

		m_pOutResultSortData = pOutSortMortonCodeData;
		m_pOutResultIdxData = pOutSortIdxData;

		m_pSecResultSortData = pInSortMortonCodeData;
		//m_pSecResultIdxData = pInSortIdxData;
	}	
}

void Diligent::BVH::DispatchMergeBitonicSortMortonCode()
{
	m_pDeviceCtx->SetPipelineState(m_apMergeBitonicSortPSO);

	Uint32 loop_idx = 0;
	Uint32 max_all_num = std::max(SortMortonCodeThreadNum << 1, m_BVHMeshData.upper_pow_of_2_primitive_num);
	for(Uint32 per_sort_num = SortMortonCodeThreadNum; per_sort_num < max_all_num; per_sort_num <<= 1)
	{
		Uint32 pass_idx = 0;
		for (Uint32 pass_sub_num = per_sort_num; pass_sub_num > 0; pass_sub_num >>= 1)
		{
			{
				MapHelper<MergeBitonicSortMortonUniformData> CBMergeBitonicSortMortonCodeData(m_pDeviceCtx, m_apMergeBitonicSortUniformData, MAP_WRITE, MAP_FLAG_DISCARD);
				CBMergeBitonicSortMortonCodeData->per_merge_code_num = per_sort_num;
				CBMergeBitonicSortMortonCodeData->pass_idx = pass_idx++;
			}

			m_apMergeBitonicSortSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "MergeBitonicSortMortonUniformData")->Set(m_apMergeBitonicSortUniformData);

			IBuffer *pInMortonCodeData;
			//IBuffer *pInIdxData;
			IBuffer *pOutSortMortonCodeData;
			IBuffer *pOutIdxData = m_pOutResultIdxData;
			if ((loop_idx++ & 1) == 0)
			{
				pInMortonCodeData = m_pOutResultSortData;
				pOutSortMortonCodeData = m_pSecResultSortData;
			}
			else
			{
				pInMortonCodeData = m_pSecResultSortData;
				pOutSortMortonCodeData = m_pOutResultSortData;
			}

			m_apMergeBitonicSortSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "BVHGlobalData")->Set(m_apGlobalBVHData);
			m_apMergeBitonicSortSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "InMortonCodeData")->Set(pInMortonCodeData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
			m_apMergeBitonicSortSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutSortMortonCodeData")->Set(pOutSortMortonCodeData->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
			m_apMergeBitonicSortSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutIdxData")->Set(pOutIdxData->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

			m_pDeviceCtx->CommitShaderResources(m_apMergeBitonicSortSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			DispatchComputeAttribs attr(std::ceilf((float)m_BVHMeshData.upper_pow_of_2_primitive_num / 2.0f / SortMortonCodeThreadNum), 1);
			m_pDeviceCtx->DispatchCompute(attr);

			m_pOutMergeResultSortData = pOutSortMortonCodeData;
		}
	}
}

void Diligent::BVH::CreateConstructBVHPSO()
{
	_CreateInitBVHNodePSO();
	_CreateContructInternalNodePSO();
	_CreateGenerateInternalNodeAABBPSO();
}

void Diligent::BVH::_CreateInitBVHNodePSO()
{
	/*ShaderMacroHelper Macros;
	Macros.AddShaderMacro("SORT_GROUP_THREADS_NUM", SortMortonCodeThreadNum);*/
	RefCntAutoPtr<IShader> pInitBVHNode = CreateShader("InitBVHNodeMain", "InitBVHNode.csh", "init bvh node cs");

	ComputePipelineStateCreateInfo PSOCreateInfo;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "BVHGlobalData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "InIdxData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "OutBVHNodeData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},		
	};
	// clang-format on
	PSOCreateInfo.PSODesc = CreatePSODescAndParam(Vars, _countof(Vars), "init bvh node pso");

	PSOCreateInfo.pCS = pInitBVHNode;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apInitBVHNodePSO);

	//SRB
	m_apInitBVHNodePSO->CreateShaderResourceBinding(&m_apInitBVHNodeSRB, true);
}

void Diligent::BVH::DispatchInitBVHNode()
{
	m_pDeviceCtx->SetPipelineState(m_apInitBVHNodePSO);

	m_apInitBVHNodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "BVHGlobalData")->Set(m_apGlobalBVHData);
	m_apInitBVHNodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "InIdxData")->Set(m_pOutResultIdxData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
	m_apInitBVHNodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutBVHNodeData")->Set(m_apBVHNodeData->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

	m_pDeviceCtx->CommitShaderResources(m_apInitBVHNodeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	DispatchComputeAttribs attr(std::ceilf((m_BVHMeshData.primitive_num * 2.0f - 1.0f) / 64), 1);
	m_pDeviceCtx->DispatchCompute(attr);
}

void Diligent::BVH::_CreateContructInternalNodePSO()
{	
	RefCntAutoPtr<IShader> pConstructInternalNode = CreateShader("ConstructInternalBVHNodeMain", "ConstructInternalBVHNode.csh", "construct internal node cs");

	ComputePipelineStateCreateInfo PSOCreateInfo;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "BVHGlobalData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "InSortMortonCode", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "OutBVHNodeData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
	};
	// clang-format on
	PSOCreateInfo.PSODesc = CreatePSODescAndParam(Vars, _countof(Vars), "construct bvh internal node pso");

	PSOCreateInfo.pCS = pConstructInternalNode;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apConstructInternalNodePSO);

	//SRB
	m_apConstructInternalNodePSO->CreateShaderResourceBinding(&m_apConstructInternalNodeSRB, true);
}

void Diligent::BVH::DispatchConstructBVHInternalNode()
{
	m_pDeviceCtx->SetPipelineState(m_apConstructInternalNodePSO);

	m_apConstructInternalNodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "BVHGlobalData")->Set(m_apGlobalBVHData);
	m_apConstructInternalNodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "InSortMortonCode")->Set(m_pOutMergeResultSortData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
	m_apConstructInternalNodeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutBVHNodeData")->Set(m_apBVHNodeData->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

	m_pDeviceCtx->CommitShaderResources(m_apConstructInternalNodeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	DispatchComputeAttribs attr(std::ceilf((m_BVHMeshData.primitive_num - 1.0f) / 64), 1);
	m_pDeviceCtx->DispatchCompute(attr);
}

void Diligent::BVH::_CreateGenerateInternalNodeAABBPSO()
{
	RefCntAutoPtr<IShader> pGenerateInternalNodeAABB = CreateShader("GenerateInternalNodeAABBMain", "GenerateInternalNodeAABB.csh", "generate internal node aabb cs");

	ComputePipelineStateCreateInfo PSOCreateInfo;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "BVHGlobalData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "InBVHNodeData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "InOutFlag", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "OutAABB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
	};
	// clang-format on
	PSOCreateInfo.PSODesc = CreatePSODescAndParam(Vars, _countof(Vars), "generate internal node aabb pso");

	PSOCreateInfo.pCS = pGenerateInternalNodeAABB;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apGenerateInternalNodeAABBPSO);

	//SRB
	m_apGenerateInternalNodeAABBPSO->CreateShaderResourceBinding(&m_apGenerateInternalNodeAABBSRB, true);
}

void Diligent::BVH::DispatchGenerateInternalNodeAABB()
{
	m_pDeviceCtx->SetPipelineState(m_apGenerateInternalNodeAABBPSO);

	m_apGenerateInternalNodeAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "BVHGlobalData")->Set(m_apGlobalBVHData);
	m_apGenerateInternalNodeAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "InBVHNodeData")->Set(m_apBVHNodeData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
	m_apGenerateInternalNodeAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "InOutFlag")->Set(m_apGenerateInternalNodeFlagData->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
	m_apGenerateInternalNodeAABBSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutAABB")->Set(m_apPrimAABBData->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

	m_pDeviceCtx->CommitShaderResources(m_apGenerateInternalNodeAABBSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	DispatchComputeAttribs attr(std::ceilf((m_BVHMeshData.primitive_num - 1.0f) / 64), 1);
	m_pDeviceCtx->DispatchCompute(attr);
}

#if DILIGENT_DEBUG

void Diligent::BVH::CreateDebugData()
{
	BufferDesc DebugBuffer;
	DebugBuffer.Name = "debug bvh buffer";
	DebugBuffer.Usage = USAGE_DEFAULT;
	DebugBuffer.ElementByteStride = sizeof(DebugBVHData);
	DebugBuffer.Mode = BUFFER_MODE_STRUCTURED;
	DebugBuffer.uiSizeInBytes = sizeof(DebugBVHData);
	DebugBuffer.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;

	DebugBVHData debug_bvh_data;
	debug_bvh_data.unorder_num_idx = -1;
	BufferData InitData;
	InitData.pData = &debug_bvh_data;
	InitData.DataSize = sizeof(DebugBVHData);
	m_pDevice->CreateBuffer(DebugBuffer, &InitData, &m_apDebugBVHData);

	//init stage buffer
	BufferDesc DebugStageBuffer;
	DebugStageBuffer.Name = "debug bvh staging buffer";
	DebugStageBuffer.Usage = USAGE_STAGING;
	DebugStageBuffer.BindFlags = BIND_NONE;
	DebugStageBuffer.Mode = BUFFER_MODE_UNDEFINED;
	DebugStageBuffer.CPUAccessFlags = CPU_ACCESS_READ;
	DebugStageBuffer.uiSizeInBytes = sizeof(DebugBVHData);
	DebugStageBuffer.ElementByteStride = sizeof(DebugBVHData);
	m_pDevice->CreateBuffer(DebugStageBuffer, nullptr, &m_apDebugBVHStageData);
}

void Diligent::BVH::CreateDebugPSO()
{
	RefCntAutoPtr<IShader> pDebugBVHDataShader = CreateShader("DebugBVHDataMain", "DebugBVHData.csh", "debug bvh data cs");

	ComputePipelineStateCreateInfo PSOCreateInfo;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "BVHGlobalData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "SortMortonCodeNums", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "OutDebugData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
	};
	// clang-format on
	PSOCreateInfo.PSODesc = CreatePSODescAndParam(Vars, _countof(Vars), "debug bvh pso");

	PSOCreateInfo.pCS = pDebugBVHDataShader;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apDebugBVHPSO);

	//SRB
	m_apDebugBVHPSO->CreateShaderResourceBinding(&m_apDebugBVHSRB, true);
}

void Diligent::BVH::DispatchDebugBVH()
{
	m_pDeviceCtx->SetPipelineState(m_apDebugBVHPSO);

	m_apDebugBVHSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "BVHGlobalData")->Set(m_apGlobalBVHData);
	m_apDebugBVHSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "SortMortonCodeNums")->Set(m_pOutMergeResultSortData->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
	m_apDebugBVHSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutDebugData")->Set(m_apDebugBVHData->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

	m_pDeviceCtx->CommitShaderResources(m_apDebugBVHSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	DispatchComputeAttribs attr(std::ceilf((m_BVHMeshData.primitive_num - 1.0f) / 64), 1);
	m_pDeviceCtx->DispatchCompute(attr);

	//read back gpu result
	m_pDeviceCtx->CopyBuffer(m_apDebugBVHData, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
		m_apDebugBVHStageData, 0, sizeof(DebugBVHData),
		RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	//sync gpu finish copy operation.
	m_pDeviceCtx->WaitForIdle();

	MapHelper<DebugBVHData> map_plant_type_num_data(m_pDeviceCtx, m_apDebugBVHStageData, MAP_READ, MAP_FLAG_DO_NOT_WAIT);

	DebugBVHData result_data;
	memcpy(&result_data, &(*map_plant_type_num_data), sizeof(DebugBVHData));
	assert(result_data.unorder_num_idx < 0);
}

#endif