#include "GroundMesh.h"
#include "FirstPersonCamera.hpp"
#include "MapHelper.hpp"
#include "TerrainMap.h"

#include "ShaderUniformDataMgr.h"
#include "OceanWave.h"

namespace Diligent
{

Diligent::GroundMesh::GroundMesh(const uint SizeM, const uint Level, const float ClipScale) :
	m_sizem(SizeM),
	m_level(Level),
	m_clip_scale(ClipScale),
	m_pVerticesData(NULL),
	m_pIndicesData(NULL),
	m_VertexNum(0),
	m_IndexNum(0),
	mpCDLODTree(nullptr),
	m_indexEndTL(0),
	m_indexEndTR(0),
	m_indexEndBL(0),
	m_indexEndBR(0)
{
	
}

Diligent::GroundMesh::~GroundMesh()
{
	if (m_pVerticesData)
	{
		delete[] m_pVerticesData;
		m_pVerticesData = NULL;
	}

	if (m_pIndicesData)
	{
		delete[] m_pIndicesData;
		m_pIndicesData = NULL;
	}

	if (mpCDLODTree)
	{
		delete mpCDLODTree;
		mpCDLODTree = nullptr;
	}
}

//void Diligent::GroundMesh::UpdateLevelOffset(const float2& CamPosXZ)
//{
//	m_LevelOffsets.resize(m_level);
//	for (int i = 0; i < m_LevelOffsets.size(); ++i)
//	{
//		m_LevelOffsets[i] = GetOffsetLevel(CamPosXZ, i);
//	}
//}

void Diligent::GroundMesh::Render(IDeviceContext *pContext, const float3 &CamPos)
{
	// Bind vertex and index buffers
	Uint32   offset = 0;
	IBuffer* pBuffs[] = { m_pVertexGPUBuffer };
	pContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
	pContext->SetIndexBuffer(m_pIndexGPUBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	// Set the pipeline state in the immediate context
	pContext->SetPipelineState(m_pPSO);
	
	//CBConstants->MeshGridUnit.x = LOD_MESH_GRID_SIZE;

	const SelectionInfo &SelectInfo = mpCDLODTree->GetSelectInfo();
	//LOG_INFO_MESSAGE("Select Node Number = ", SelectInfo.SelectionNodes.size());

	for (int i = 0; i < SelectInfo.SelectionNodes.size(); ++i)
	{
		const SelectNodeData &NodeData = SelectInfo.SelectionNodes[i];


		// Set uniform
		{
			// Map the buffer and write current world-view-projection matrix
			MapHelper<GPUConstBuffer> CBConstants(pContext, m_pVsConstBuf, MAP_WRITE, MAP_FLAG_DISCARD);
			CBConstants->ViewProj = m_TerrainViewProjMat;

			int ShaderLODLevel = LOD_COUNT - NodeData.pNode->LODLevel - 1;
			MapHelper<PerPatchShaderData> PerPatchConstData(pContext, m_pVsPatchBuf, MAP_WRITE, MAP_FLAG_DISCARD);
			PerPatchConstData->Scale = float4((NodeData.aabb.Max.x - NodeData.aabb.Min.x) / LOD_MESH_GRID_SIZE,
				(NodeData.aabb.Max.z - NodeData.aabb.Min.z) / LOD_MESH_GRID_SIZE,
				ShaderLODLevel, 0.0f);
			PerPatchConstData->Offset = float4(NodeData.aabb.Min.x,
				(NodeData.aabb.Max.y + NodeData.aabb.Min.y) / 2.0f, NodeData.aabb.Min.z, 0.0f);

			float MorphInfo[2];
			SelectInfo.GetMorphFromLevel(ShaderLODLevel, MorphInfo);
			CBConstants->MorphKInfo = float4({ MorphInfo[0], MorphInfo[1], 0.0f, 0.0f });
			CBConstants->CameraPos = CamPos;
		}

		// Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
		// makes sure that resources are transitioned to required states.
		pContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		if (NodeData.AreaFlag.flag == SelectNodeAreaFlag::FULL)
		{
			pContext->DrawIndexed(GetDrawIndex(0, m_IndexNum));
		}
		else
		{
			uint16_t QuadIndexNum = m_IndexNum / 4;
			//Top Left
			if (NodeData.AreaFlag.flag & SelectNodeAreaFlag::TL_ON)
			{
				pContext->DrawIndexed(GetDrawIndex(0, QuadIndexNum));
			}

			//Top Right
			if (NodeData.AreaFlag.flag & SelectNodeAreaFlag::TR_ON)
			{
				pContext->DrawIndexed(GetDrawIndex(m_indexEndTL, QuadIndexNum));
			}

			//Bottom Left
			if (NodeData.AreaFlag.flag & SelectNodeAreaFlag::BL_ON)
			{
				pContext->DrawIndexed(GetDrawIndex(m_indexEndTR, QuadIndexNum));
			}

			//Bottom Right
			if (NodeData.AreaFlag.flag & SelectNodeAreaFlag::BR_ON)
			{
				pContext->DrawIndexed(GetDrawIndex(m_indexEndBL, QuadIndexNum));
			}
		}		
	}
}

DrawIndexedAttribs GroundMesh::GetDrawIndex(const uint16_t start, const uint16_t num)
{
	DrawIndexedAttribs drawAttrs;
	drawAttrs.IndexType = VT_UINT16; // Index type
	drawAttrs.FirstIndexLocation = start;
	drawAttrs.NumIndices = num;
	// Verify the state of vertex and index buffers
	drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;

	return drawAttrs;
}

void GroundMesh::InitClipMap(IRenderDevice *pDevice, ISwapChain *pSwapChain, ShaderUniformDataMgr *pShaderUniformDataMgr)
{	
	//m_Heightmap.LoadHeightMap("./wm_heightmap.png", pDevice);
	m_Heightmap.LoadMap("./assets/wm_diffuse_map.png", "./assets/wm_heightmap.png", pDevice);

	Dimension TerrainDim;
	TerrainDim.Min = float3({ -5690.0f, 0.00f, -7090.0f });
	TerrainDim.Size = float3({ 12000.0f, 1000.0f, 12000.0f });
	mpCDLODTree = new CDLODTree(m_Heightmap, TerrainDim);
	mpCDLODTree->Create();

	InitVertexBuffer();
	InitIndicesBuffer();

	CommitToGPUDeviceBuffer(pDevice);
	InitPSO(pDevice, pSwapChain, TerrainDim, pShaderUniformDataMgr);

	//init shader value
	IShaderResourceVariable *g_TextureVar = m_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_displacement_tex");
	if (g_TextureVar)
	{
		g_TextureVar->Set(m_Heightmap.GetHeightMapTexture()->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	}
	g_TextureVar = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_displacement_tex");
	if (g_TextureVar)
	{
		g_TextureVar->Set(m_Heightmap.GetHeightMapTexture()->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	}
	/*IShaderResourceVariable *g_DiffuseTextureVar = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_DiffTexture");
	if (g_DiffuseTextureVar)
	{
		g_DiffuseTextureVar->Set(m_Heightmap.GetDiffuseMapTexture()->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	}*/

}

void GroundMesh::Render(IDeviceContext* pContext, const float3& CamPos, const WaterRenderData &WRenderData)
{
	// Bind vertex and index buffers
	Uint32   offset = 0;
	IBuffer* pBuffs[] = { m_pVertexGPUBuffer };
	pContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
	pContext->SetIndexBuffer(m_pIndexGPUBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	// Set the pipeline state in the immediate context
	pContext->SetPipelineState(m_pPSO);

	//CBConstants->MeshGridUnit.x = LOD_MESH_GRID_SIZE;

	const SelectionInfo& SelectInfo = mpCDLODTree->GetSelectInfo();
	//LOG_INFO_MESSAGE("Select Node Number = ", SelectInfo.SelectionNodes.size());

	for (int i = 0; i < SelectInfo.SelectionNodes.size(); ++i)
	{
		const SelectNodeData& NodeData = SelectInfo.SelectionNodes[i];


		// Set uniform
		{
			// Map the buffer and write current world-view-projection matrix
			MapHelper<GPUConstBuffer> CBConstants(pContext, m_pVsConstBuf, MAP_WRITE, MAP_FLAG_DISCARD);
			CBConstants->ViewProj = m_TerrainViewProjMat;

			int ShaderLODLevel = LOD_COUNT - NodeData.pNode->LODLevel - 1;
			MapHelper<PerPatchShaderData> PerPatchConstData(pContext, m_pVsPatchBuf, MAP_WRITE, MAP_FLAG_DISCARD);
			PerPatchConstData->Scale = float4((NodeData.aabb.Max.x - NodeData.aabb.Min.x) / LOD_MESH_GRID_SIZE,
				(NodeData.aabb.Max.z - NodeData.aabb.Min.z) / LOD_MESH_GRID_SIZE,
				ShaderLODLevel, 0.0f);
			PerPatchConstData->Offset = float4(NodeData.aabb.Min.x,
				(NodeData.aabb.Max.y + NodeData.aabb.Min.y) / 2.0f, NodeData.aabb.Min.z, 0.0f);

			float MorphInfo[2];
			SelectInfo.GetMorphFromLevel(ShaderLODLevel, MorphInfo);
			CBConstants->MorphKInfo = float4({ MorphInfo[0], MorphInfo[1], 0.0f, 0.0f });
			CBConstants->CameraPos = CamPos;
			CBConstants->L.x = WRenderData.L_RepeatScale_NormalIntensity_N.x;
			CBConstants->L.y = WRenderData.L_RepeatScale_NormalIntensity_N.y;
			CBConstants->g_BaseNormalIntensity = WRenderData.L_RepeatScale_NormalIntensity_N.z;
			CBConstants->g_FFTN = WRenderData.L_RepeatScale_NormalIntensity_N.w;			

			//disp lods
			ExportRenderParams OceanRenderShaderParams = WRenderData.pOceanWave->ExportParamsToShader();
			IShaderResourceVariable *pShaderHM = m_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_displacement_texL0");
			if (pShaderHM)
			{
				pShaderHM->Set(OceanRenderShaderParams.OceanRenderTexs[0].pDisp->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
			}
			pShaderHM = m_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_displacement_texL1");
			if (pShaderHM)
			{
				pShaderHM->Set(OceanRenderShaderParams.OceanRenderTexs[1].pDisp->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
			}
			pShaderHM = m_pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_displacement_texL2");
			if (pShaderHM)
			{
				pShaderHM->Set(OceanRenderShaderParams.OceanRenderTexs[2].pDisp->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
			}
			//derivative texs
			pShaderHM = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_derivatives_texL0");
			if (pShaderHM)
			{
				pShaderHM->Set(OceanRenderShaderParams.OceanRenderTexs[0].pDeriva->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
			}
			pShaderHM = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_derivatives_texL1");
			if (pShaderHM)
			{
				pShaderHM->Set(OceanRenderShaderParams.OceanRenderTexs[1].pDeriva->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
			}
			pShaderHM = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_derivatives_texL2");
			if (pShaderHM)
			{
				pShaderHM->Set(OceanRenderShaderParams.OceanRenderTexs[2].pDeriva->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
			}
			//turbulence texs
			pShaderHM = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_turbulence_texL0");
			if (pShaderHM)
			{
				pShaderHM->Set(OceanRenderShaderParams.OceanRenderTexs[0].pTurb->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
			}
			pShaderHM = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_turbulence_texL1");
			if (pShaderHM)
			{
				pShaderHM->Set(OceanRenderShaderParams.OceanRenderTexs[1].pTurb->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
			}
			pShaderHM = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_turbulence_texL2");
			if (pShaderHM)
			{
				pShaderHM->Set(OceanRenderShaderParams.OceanRenderTexs[2].pTurb->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
			}
			//lighting tex
			pShaderHM = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_IrradianceCube");
			if (pShaderHM)
			{
				pShaderHM->Set(WRenderData.pDiffIrradianceMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
			}
			pShaderHM = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_IBLSpecCube");
			if (pShaderHM)
			{
				pShaderHM->Set(WRenderData.pIBLSPecMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
			}
			CBConstants->LengthScale0 = OceanRenderShaderParams.LengthScales[0];
			CBConstants->LengthScale1 = OceanRenderShaderParams.LengthScales[1];
			CBConstants->LengthScale2 = OceanRenderShaderParams.LengthScales[2];
			CBConstants->LOD_scale = 7.0f;
		}
		{
			//Ocean render material params
			MapHelper<OceanMaterialParams> OceanRMatParams(pContext, m_pPsOceanMatParamBuf, MAP_WRITE, MAP_FLAG_DISCARD);
			*OceanRMatParams = WRenderData.OceanRMatParams;
		}

		// Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
		// makes sure that resources are transitioned to required states.
		pContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		if (NodeData.AreaFlag.flag == SelectNodeAreaFlag::FULL)
		{
			pContext->DrawIndexed(GetDrawIndex(0, m_IndexNum));
		}
		else
		{
			uint16_t QuadIndexNum = m_IndexNum / 4;
			//Top Left
			if (NodeData.AreaFlag.flag & SelectNodeAreaFlag::TL_ON)
			{
				pContext->DrawIndexed(GetDrawIndex(0, QuadIndexNum));
			}

			//Top Right
			if (NodeData.AreaFlag.flag & SelectNodeAreaFlag::TR_ON)
			{
				pContext->DrawIndexed(GetDrawIndex(m_indexEndTL, QuadIndexNum));
			}

			//Bottom Left
			if (NodeData.AreaFlag.flag & SelectNodeAreaFlag::BL_ON)
			{
				pContext->DrawIndexed(GetDrawIndex(m_indexEndTR, QuadIndexNum));
			}

			//Bottom Right
			if (NodeData.AreaFlag.flag & SelectNodeAreaFlag::BR_ON)
			{
				pContext->DrawIndexed(GetDrawIndex(m_indexEndBL, QuadIndexNum));
			}
		}
	}
}

void GroundMesh::Update(const FirstPersonCamera *pCam)
{
	//const float3 &pos = pCam->GetPos();
	//UpdateLevelOffset(float2(pos.x, pos.z));
	//UpdateLevelOffset(float2(0.0f, 0.0f));

	m_TerrainViewProjMat = pCam->GetViewProjMatrix();

	mpCDLODTree->SelectLOD(*pCam);
}

void GroundMesh::CommitToGPUDeviceBuffer(IRenderDevice *pDevice)
{
	//vertex
	BufferDesc VertBufDesc;
	VertBufDesc.Name = "ClipMap Terrain Vertex Buffer";
	VertBufDesc.Usage = USAGE_IMMUTABLE;
	VertBufDesc.BindFlags = BIND_VERTEX_BUFFER;
	VertBufDesc.uiSizeInBytes = sizeof(ClipMapTerrainVerticesData) * m_VertexNum;

	BufferData VBData;
	VBData.pData = m_pVerticesData;
	VBData.DataSize = sizeof(ClipMapTerrainVerticesData) * m_VertexNum;
	pDevice->CreateBuffer(VertBufDesc, &VBData, &m_pVertexGPUBuffer);

	//index
	BufferDesc IdxBufDesc;
	IdxBufDesc.Name = "ClipMap Terrain Index Buffer";
	IdxBufDesc.Usage = USAGE_IMMUTABLE;
	IdxBufDesc.BindFlags = BIND_INDEX_BUFFER;
	IdxBufDesc.uiSizeInBytes = sizeof(uint16_t) * m_IndexNum;

	BufferData IdxData;
	IdxData.pData = m_pIndicesData;
	IdxData.DataSize = sizeof(uint16_t) * m_IndexNum;
	pDevice->CreateBuffer(IdxBufDesc, &IdxData, &m_pIndexGPUBuffer);
}

void GroundMesh::InitPSO(IRenderDevice *pDevice, ISwapChain *pSwapChain, const Dimension& dim, ShaderUniformDataMgr *pShaderUniformDataMgr)
{	
	// Pipeline state object encompasses configuration of all GPU stages

	GraphicsPipelineStateCreateInfo PSOCreateInfo;

	PipelineStateDesc& PSODesc = PSOCreateInfo.PSODesc;
	PipelineResourceLayoutDesc& ResourceLayout = PSODesc.ResourceLayout;

	// Pipeline state name is used by the engine to report issues.
	// It is always a good idea to give objects descriptive names.
	PSOCreateInfo.PSODesc.Name = "Water Mesh PSO";

	// This is a graphics pipeline
	PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

	// clang-format off
	// This tutorial will render to a single render target
	PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
	// Set render target format which is the format of the swap chain's color buffer
	PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_R11G11B10_FLOAT;//pSwapChain->GetDesc().ColorBufferFormat;
	// Use the depth buffer format from the swap chain
	PSOCreateInfo.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;// pSwapChain->GetDesc().DepthBufferFormat;
	// Primitive topology defines what kind of primitives will be rendered by this pipeline state
	PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// Wireframe
	//PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FillMode = FILL_MODE_WIREFRAME;
	PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FillMode = FILL_MODE_SOLID;

	// No back face culling for this tutorial
	PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
	// depth testing
	PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;

	// Define vertex shader input layout
	LayoutElement LayoutElems[] =
	{
		// Attribute 0 - vertex position
		LayoutElement{0, 0, 2, VT_FLOAT32, False},		
	};
	// clang-format on
	PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
	PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

	// clang-format on

	ShaderCreateInfo ShaderCI;
	RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
	pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
	ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
	// Tell the system that the shader source code is in HLSL.
	// For OpenGL, the engine will convert this into GLSL under the hood.
	ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	// OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
	ShaderCI.UseCombinedTextureSamplers = true;
	// Create a vertex shader
	RefCntAutoPtr<IShader> pVS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
		ShaderCI.EntryPoint = "main";
		ShaderCI.Desc.Name = "ClipMap Terrain vertex shader";
		//ShaderCI.Source          = VSSource;
		ShaderCI.FilePath = "assets/clipmap.vsh";
		pDevice->CreateShader(ShaderCI, &pVS);

		//Create dynamic const buffer		
		BufferDesc CBDesc;
		CBDesc.Name = "ClipMap VS Constants CB";
		CBDesc.uiSizeInBytes = sizeof(GPUConstBuffer);
		CBDesc.Usage = USAGE_DYNAMIC;
		CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
		CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
		pDevice->CreateBuffer(CBDesc, nullptr, &m_pVsConstBuf);

		struct GPUDIM
		{
			float4 Min;
			float4 Size;
		};
		GPUDIM gdim;
		gdim.Min = dim.Min;
		gdim.Size = dim.Size;

		BufferDesc TerrainDesc;
		TerrainDesc.Name = "ClipMap VS Terrain info CB";
		TerrainDesc.uiSizeInBytes = sizeof(GPUDIM);
		TerrainDesc.Usage = USAGE_IMMUTABLE;
		TerrainDesc.BindFlags = BIND_UNIFORM_BUFFER;
		BufferData TerrainInitData;
		TerrainInitData.pData = &gdim;
		TerrainInitData.DataSize = TerrainDesc.uiSizeInBytes;
		pDevice->CreateBuffer(TerrainDesc, &TerrainInitData, &m_pVSTerrainInfoBuf);

		//Create dynamic patch buffer
		BufferDesc PatchCBDesc;
		PatchCBDesc.Name = "ClipMap VS Patch CB";
		PatchCBDesc.uiSizeInBytes = sizeof(PerPatchShaderData);
		PatchCBDesc.Usage = USAGE_DYNAMIC;
		PatchCBDesc.BindFlags = BIND_UNIFORM_BUFFER;
		PatchCBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
		pDevice->CreateBuffer(PatchCBDesc, nullptr, &m_pVsPatchBuf);		
	}

	// Create a pixel shader
	RefCntAutoPtr<IShader> pPS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
		ShaderCI.EntryPoint = "main";
		ShaderCI.Desc.Name = "ClipMap Terrain pixel shader";
		//ShaderCI.Source          = PSSource;
		ShaderCI.FilePath = "assets/clipmap.psh";
		pDevice->CreateShader(ShaderCI, &pPS);

		BufferDesc OceanMatParamsDesc;
		OceanMatParamsDesc.Name = "Ocean material params CB";
		OceanMatParamsDesc.uiSizeInBytes = sizeof(OceanMaterialParams);
		OceanMatParamsDesc.Usage = USAGE_DYNAMIC;
		OceanMatParamsDesc.BindFlags = BIND_UNIFORM_BUFFER;
		OceanMatParamsDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
		pDevice->CreateBuffer(OceanMatParamsDesc, nullptr, &m_pPsOceanMatParamBuf);
	}

	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	// clang-format off
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_VERTEX, "g_displacement_texL0", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
		{SHADER_TYPE_VERTEX, "g_displacement_texL1", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
		{SHADER_TYPE_VERTEX, "g_displacement_texL2", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
		{SHADER_TYPE_PIXEL, "g_derivatives_texL0", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
		{SHADER_TYPE_PIXEL, "g_derivatives_texL1", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
		{SHADER_TYPE_PIXEL, "g_derivatives_texL2", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
		{SHADER_TYPE_PIXEL, "g_turbulence_texL0", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
		{SHADER_TYPE_PIXEL, "g_turbulence_texL1", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
		{SHADER_TYPE_PIXEL, "g_turbulence_texL2", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
		{SHADER_TYPE_PIXEL, "g_IrradianceCube", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
		{SHADER_TYPE_PIXEL, "g_IBLSpecCube", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
	};
	// clang-format on
	ResourceLayout.Variables = Vars;
	ResourceLayout.NumVariables = _countof(Vars);

	// Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
	// clang-format off
	SamplerDesc SamLinearWrapDesc
	{
		FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
		TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP
	};
	SamplerDesc SamAnisoWrapDesc
	{
		FILTER_TYPE_ANISOTROPIC, FILTER_TYPE_ANISOTROPIC, FILTER_TYPE_ANISOTROPIC,
		TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP
	};
	ImmutableSamplerDesc ImtblSamplers[] =
	{
		{SHADER_TYPE_VERTEX, "g_displacement_texL0", SamAnisoWrapDesc},
		{SHADER_TYPE_VERTEX, "g_displacement_texL1", SamAnisoWrapDesc},
		{SHADER_TYPE_VERTEX, "g_displacement_texL2", SamAnisoWrapDesc},
		{SHADER_TYPE_PIXEL, "g_derivatives_texL0", SamAnisoWrapDesc},
		{SHADER_TYPE_PIXEL, "g_derivatives_texL1", SamAnisoWrapDesc},
		{SHADER_TYPE_PIXEL, "g_derivatives_texL2", SamAnisoWrapDesc},
		{SHADER_TYPE_PIXEL, "g_turbulence_texL0", SamAnisoWrapDesc},
		{SHADER_TYPE_PIXEL, "g_turbulence_texL1", SamAnisoWrapDesc},
		{SHADER_TYPE_PIXEL, "g_turbulence_texL2", SamAnisoWrapDesc},
		{SHADER_TYPE_PIXEL, "g_IrradianceCube", SamLinearWrapDesc},
		{SHADER_TYPE_PIXEL, "g_IBLSpecCube", SamLinearWrapDesc}
	};
	// clang-format on
	ResourceLayout.ImmutableSamplers = ImtblSamplers;
	ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

	// Finally, create the pipeline state
	PSOCreateInfo.pVS = pVS;
	PSOCreateInfo.pPS = pPS;
	pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);

	// Since we did not explcitly specify the type for 'Constants' variable, default
	// type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
	// change and are bound directly through the pipeline state object.
	m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pVsConstBuf);
	m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "TerrainDimension")->Set(m_pVSTerrainInfoBuf);
	m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "PerPatchData")->Set(m_pVsPatchBuf);	
	m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "OceanMaterialParams")->Set(m_pPsOceanMatParamBuf);
	m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "Constants")->Set(m_pVsConstBuf);
	m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbLightStructure")->Set(pShaderUniformDataMgr->GetLightStructure());
	m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "OceanMaterialParams")->Set(m_pPsOceanMatParamBuf);

	// Create a shader resource binding object and bind all static resources in it
	m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);

}

void GroundMesh::InitVertexBuffer()
{
	uint VertexSizeM = m_sizem + 1;
	uint VertexNum = VertexSizeM * VertexSizeM;
		
	m_pVerticesData = new ClipMapTerrainVerticesData[VertexNum];
	ClipMapTerrainVerticesData *pCurr = m_pVerticesData;

	//Init Block
	for (uint i = 0; i < VertexSizeM; ++i)
	{
		for (uint j = 0; j < VertexSizeM; ++j)
		{
			pCurr->XZ = float2((float)j, (float)i);
			++pCurr;
		}
	}

	m_VertexNum += VertexNum;
}

void GroundMesh::InitIndicesBuffer()
{		
	uint NumIndex = m_sizem * m_sizem * 2 * 3;
	m_IndexNum = NumIndex;

	m_pIndicesData = new uint16_t[NumIndex];
	memset(m_pIndicesData, 0, sizeof(uint16_t) * NumIndex);

	uint16_t *pCurr = m_pIndicesData;
	
	int index = 0;

	int VertDim = m_sizem + 1;
	int halfd = (VertDim / 2);
	int fulld = m_sizem;

	// Top left part
	for (int y = 0; y < halfd; y++)
	{
		for (int x = 0; x < halfd; x++)
		{
			pCurr[index++] = (uint16_t)(x + VertDim * (y + 1)); 
			pCurr[index++] = (uint16_t)((x + 1) + VertDim * y);     
			pCurr[index++] = (uint16_t)(x + VertDim * y);

			pCurr[index++] = (uint16_t)(x + VertDim * (y + 1)); 
			pCurr[index++] = (uint16_t)((x + 1) + VertDim * (y + 1)); 
			pCurr[index++] = (uint16_t)((x + 1) + VertDim * y);
		}
	}
	m_indexEndTL = index;

	// Top right part
	for (int y = 0; y < halfd; y++)
	{
		for (int x = halfd; x < fulld; x++)
		{
			pCurr[index++] = (uint16_t)(x + VertDim * (y + 1)); 
			pCurr[index++] = (uint16_t)((x + 1) + VertDim * y);     
			pCurr[index++] = (uint16_t)(x + VertDim * y);

			pCurr[index++] = (uint16_t)(x + VertDim * (y + 1)); 
			pCurr[index++] = (uint16_t)((x + 1) + VertDim * (y + 1)); 
			pCurr[index++] = (uint16_t)((x + 1) + VertDim * y);
		}
	}
	m_indexEndTR = index;

	// Bottom left part
	for (int y = halfd; y < fulld; y++)
	{
		for (int x = 0; x < halfd; x++)
		{
			pCurr[index++] = (uint16_t)(x + VertDim * (y + 1)); 
			pCurr[index++] = (uint16_t)((x + 1) + VertDim * y);     
			pCurr[index++] = (uint16_t)(x + VertDim * y);

			pCurr[index++] = (uint16_t)(x + VertDim * (y + 1)); 
			pCurr[index++] = (uint16_t)((x + 1) + VertDim * (y + 1)); 
			pCurr[index++] = (uint16_t)((x + 1) + VertDim * y);
		}
	}
	m_indexEndBL = index;

	// Bottom right part
	for (int y = halfd; y < fulld; y++)
	{
		for (int x = halfd; x < fulld; x++)
		{
			pCurr[index++] = (uint16_t)(x + VertDim * (y + 1)); 
			pCurr[index++] = (uint16_t)((x + 1) + VertDim * y);     
			pCurr[index++] = (uint16_t)(x + VertDim * y);

			pCurr[index++] = (uint16_t)(x + VertDim * (y + 1)); 
			pCurr[index++] = (uint16_t)((x + 1) + VertDim * (y + 1)); 
			pCurr[index++] = (uint16_t)((x + 1) + VertDim * y);
		}
	}
	m_indexEndBR = index;
}

}

