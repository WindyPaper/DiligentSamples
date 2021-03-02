#include "GroundMesh.h"

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

void Diligent::GroundMesh::UpdateLevelOffset(const float2 CamPosXZ)
{

}

void Diligent::GroundMesh::Render(IDeviceContext *Device)
{

}

void GroundMesh::InitClipMap(IRenderDevice *pDevice)
{
	InitVertexBuffer();
	InitIndicesBuffer();

	CommitToGPUDeviceBuffer(pDevice);
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
	PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

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

		//Create dynamic buffer
		BufferDesc CBDesc;
		CBDesc.Name = "ClipMap VS Constants CB";
		CBDesc.uiSizeInBytes = sizeof(float4x4);
		CBDesc.Usage = USAGE_DYNAMIC;
		CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
		CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
		pDevice->CreateBuffer(CBDesc, nullptr, &m_pVsConstBuf);
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

	// Create a shader resource binding object and bind all static resources in it
	m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);

}

void GroundMesh::InitVertexBuffer()
{
	uint VertexNum = m_sizem * m_sizem;
		
	m_pVerticesData = new ClipMapTerrainVerticesData[VertexNum];
	ClipMapTerrainVerticesData *pCurr = m_pVerticesData;

	//Init Block
	for (int i = 0; i < m_sizem; ++i)
	{
		for (int j = 0; j < m_sizem; ++j)
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

int GroundMesh::PatchIndexCount(const uint sizex, const uint sizez)
{
	uint strips = sizez - 1;
	return strips * (2 * sizex - 1) + 1;
}



}

