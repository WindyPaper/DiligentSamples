#include "GroundMesh.h"
#include "FirstPersonCamera.hpp"
#include "MapHelper.hpp"

namespace Diligent
{

Diligent::GroundMesh::GroundMesh(const uint SizeM, const uint Level, const float ClipScale) :
	m_sizem(SizeM),
	m_level(Level),
	m_clip_scale(ClipScale),
	m_pVerticesData(NULL),
	m_pIndicesData(NULL),
	m_VertexNum(0),
	m_IndexNum(0)
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
}

void Diligent::GroundMesh::UpdateLevelOffset(const float2& CamPosXZ)
{
	m_LevelOffsets.resize(m_level);
	for (int i = 0; i < m_LevelOffsets.size(); ++i)
	{
		m_LevelOffsets[i] = GetOffsetLevel(CamPosXZ, i);
	}
}

void Diligent::GroundMesh::Render(IDeviceContext *pContext)
{
	// Bind vertex and index buffers
	Uint32   offset = 0;
	IBuffer* pBuffs[] = { m_pVertexGPUBuffer };
	pContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
	pContext->SetIndexBuffer(m_pIndexGPUBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	// Set the pipeline state in the immediate context
	pContext->SetPipelineState(m_pPSO);
	
	// Map the buffer and write current world-view-projection matrix
	MapHelper<float4x4> CBConstants(pContext, m_pVsConstBuf, MAP_WRITE, MAP_FLAG_DISCARD);
	*CBConstants = m_TerrainViewProjMat;

	//Level 0
	for (unsigned int z = 0; z < 4; z++)
	{
		for (unsigned int x = 0; x < 4; x++)
		{
			// Set uniform
			{
				MapHelper<PerPatchShaderData> PerPatchConstData(pContext, m_pVsPatchBuf, MAP_WRITE, MAP_FLAG_DISCARD);
				PerPatchConstData->Level = 0;
				PerPatchConstData->Scale = m_clip_scale;
				PerPatchConstData->Offset = m_LevelOffsets[0] + float2(x * (m_sizem - 1), z * (m_sizem - 1));
				PerPatchConstData->Offset *= m_clip_scale;
			}

			// Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
			// makes sure that resources are transitioned to required states.
			pContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			DrawIndexedAttribs drawAttrs;
			drawAttrs.IndexType = VT_UINT16; // Index type
			drawAttrs.NumIndices = m_IndexNum;
			// Verify the state of vertex and index buffers
			drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
			pContext->DrawIndexed(drawAttrs);
		}
	}

	for (int LvIdx = 1; LvIdx < m_level; ++LvIdx)
	{
		for (unsigned int z = 0; z < 4; z++)
		{
			for (unsigned int x = 0; x < 4; x++)
			{
				if (z != 0 && z != 3 && x != 0 && x != 3)
				{
					continue;
				}

				// Set uniform
				{					
					MapHelper<PerPatchShaderData> PerPatchConstData(pContext, m_pVsPatchBuf, MAP_WRITE, MAP_FLAG_DISCARD);
					PerPatchConstData->Level = LvIdx;
					PerPatchConstData->Scale = m_clip_scale * (1 << LvIdx);
					PerPatchConstData->Offset = m_LevelOffsets[LvIdx] + float2(x * (m_sizem - 1), z * (m_sizem - 1)) * (1 << LvIdx);
					PerPatchConstData->Offset *= m_clip_scale;
				}

				// Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
				// makes sure that resources are transitioned to required states.
				pContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

				DrawIndexedAttribs drawAttrs;
				drawAttrs.IndexType = VT_UINT16; // Index type
				drawAttrs.NumIndices = m_IndexNum;
				// Verify the state of vertex and index buffers
				drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
				pContext->DrawIndexed(drawAttrs);
			}
		}
	}
}

void GroundMesh::InitClipMap(IRenderDevice *pDevice, ISwapChain *pSwapChain)
{
	InitVertexBuffer();
	InitIndicesBuffer();

	CommitToGPUDeviceBuffer(pDevice);
	InitPSO(pDevice, pSwapChain);
}

void GroundMesh::Update(const FirstPersonCamera *pCam)
{
	const float3 &pos = pCam->GetPos();
	UpdateLevelOffset(float2(pos.x, pos.z));

	m_TerrainViewProjMat = Matrix4x4<float>::Identity() * pCam->GetViewMatrix() * pCam->GetProjMatrix();
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

void GroundMesh::InitPSO(IRenderDevice *pDevice, ISwapChain *pSwapChain)
{	
	// Pipeline state object encompasses configuration of all GPU stages

	GraphicsPipelineStateCreateInfo PSOCreateInfo;

	// Pipeline state name is used by the engine to report issues.
	// It is always a good idea to give objects descriptive names.
	PSOCreateInfo.PSODesc.Name = "Simple triangle PSO";

	// This is a graphics pipeline
	PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

	// clang-format off
	// This tutorial will render to a single render target
	PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
	// Set render target format which is the format of the swap chain's color buffer
	PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = pSwapChain->GetDesc().ColorBufferFormat;
	// Use the depth buffer format from the swap chain
	PSOCreateInfo.GraphicsPipeline.DSVFormat = pSwapChain->GetDesc().DepthBufferFormat;
	// Primitive topology defines what kind of primitives will be rendered by this pipeline state
	PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	// Wireframe
	PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FillMode = FILL_MODE_WIREFRAME;

	// No back face culling for this tutorial
	PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
	// Disable depth testing
	PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

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
		ShaderCI.FilePath = "clipmap.vsh";
		pDevice->CreateShader(ShaderCI, &pVS);

		//Create dynamic const buffer
		BufferDesc CBDesc;
		CBDesc.Name = "ClipMap VS Constants CB";
		CBDesc.uiSizeInBytes = sizeof(float4x4);
		CBDesc.Usage = USAGE_DYNAMIC;
		CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
		CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
		pDevice->CreateBuffer(CBDesc, nullptr, &m_pVsConstBuf);

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
		ShaderCI.FilePath = "clipmap.psh";
		pDevice->CreateShader(ShaderCI, &pPS);
	}

	// Finally, create the pipeline state
	PSOCreateInfo.pVS = pVS;
	PSOCreateInfo.pPS = pPS;
	pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);

	// Since we did not explcitly specify the type for 'Constants' variable, default
	// type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
	// change and are bound directly through the pipeline state object.
	m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pVsConstBuf);
	m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "PerPatchData")->Set(m_pVsPatchBuf);

	// Create a shader resource binding object and bind all static resources in it
	m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);

}

void GroundMesh::InitVertexBuffer()
{
	uint VertexNum = m_sizem * m_sizem;
		
	m_pVerticesData = new ClipMapTerrainVerticesData[VertexNum];
	ClipMapTerrainVerticesData *pCurr = m_pVerticesData;

	//Init Block
	for (uint i = 0; i < m_sizem; ++i)
	{
		for (uint j = 0; j < m_sizem; ++j)
		{
			pCurr->XZ = float2((float)j, (float)i);
			++pCurr;
		}
	}

	m_VertexNum += VertexNum;
}

void GroundMesh::InitIndicesBuffer()
{	
	uint vertex_buffer_offset = 0;
	
	m_block.Count = PatchIndexCount(m_sizem, m_sizem);

	uint NumIndex = m_block.Count;
	m_IndexNum = NumIndex;
	m_pIndicesData = new uint16_t[NumIndex];
	memset(m_pIndicesData, 0, sizeof(uint16_t) * NumIndex);

	uint16_t *pCurr = m_pIndicesData;

	m_block.Offset = (uint)(pCurr - m_pIndicesData);
	pCurr = GeneratePatchIndices(pCurr, vertex_buffer_offset, m_sizem, m_sizem);

	vertex_buffer_offset += m_sizem * m_sizem;
}

uint16_t* GroundMesh::GeneratePatchIndices(uint16_t *pIndexdata, uint VertexOffset, uint SizeX, uint SizeZ)
{
	uint VertexPos = VertexOffset;

	uint Strips = SizeZ - 1;

	// After even indices in a strip, always step to next strip.
	// After odd indices in a strip, step back again and one to the right or left.
	// Which direction we take depends on which strip we're generating.
	// This creates a zig-zag pattern.
	for (uint z = 0; z < Strips; z++)
	{
		int StepEven = SizeX;
		int StepOdd = ((z & 1) ? -1 : 1) - StepEven;

		// We don't need the last odd index.
		// The first index of the next strip will complete this strip.
		for (uint x = 0; x < 2 * SizeX - 1; x++)
		{
			*pIndexdata++ = VertexPos;
			VertexPos += (x & 1) ? StepOdd : StepEven;
		}
	}
	// There is no new strip, so complete the block here.
	*pIndexdata++ = VertexPos;

	// Return updated index buffer pointer.
	// More explicit than taking reference to pointer.
	return pIndexdata;
}

int GroundMesh::PatchIndexCount(const uint sizex, const uint sizez) const
{
	uint strips = sizez - 1;
	return strips * (2 * sizex - 1) + 1;
}



Diligent::float2 GroundMesh::GetOffsetLevel(const float2 &CamPosXZ, const uint Level) const
{
	if (Level == 0) // Must follow level 1 as trim region is fixed.
	{
		return GetOffsetLevel(CamPosXZ, 1) + float2(float(m_sizem << 1));
	}
	else
	{
		float2 ScalePos = CamPosXZ / float2(m_clip_scale); // Snap to grid in the appropriate space.

		// Snap to grid of next level. I.e. we move the clipmap level in steps of two.
		float2 SnappedPos = (ScalePos / float2(1 << (Level + 1))) * float2(1 << (Level + 1));
		SnappedPos = float2(std::floorf(SnappedPos.x), std::floorf(SnappedPos.y));

		// Apply offset so all levels align up neatly.
		// If SnappedPos is equal for all levels,
		// this causes top-left vertex of level N to always align up perfectly with top-left interior corner of level N + 1.
		// This gives us a bottom-right trim region.

		// Due to the flooring, SnappedPos might not always be equal for all levels.
		// The flooring has the property that SnappedPos for level N + 1 is less-or-equal SnappedPos for level N.
		// If less, the final position of level N + 1 will be offset by -2 ^ N, which can be compensated for with changing trim-region to top-left.
		float2 pos = SnappedPos - float2((2 * (m_sizem - 1)) << Level);
		return pos;
	}
}

}

