#include "DebugCanvas.h"
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "FirstPersonCamera.hpp"

namespace Diligent
{

	DebugCanvas::DebugCanvas() : 
		mbInitGPUBuffer(false)
	{

	}

	DebugCanvas::~DebugCanvas()
	{

	}


	void DebugCanvas::Draw(IRenderDevice *pDevice, ISwapChain *pSwapChain, IDeviceContext *pContext, const FirstPersonCamera *cam)
	{
		if (mbInitGPUBuffer == false)
		{
			CreateVertexBuffer(pDevice);
			CreateIndexBuffer(pDevice);
			
		}

		auto* pRTV = pSwapChain->GetCurrentBackBufferRTV();
		auto* pDSV = pSwapChain->GetDepthBufferDSV();
		// Clear the back buffer
		const float ClearColor[] = { 0.350f, 0.350f, 0.350f, 1.0f };
		pContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		pContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		{
			// Map the buffer and write current world-view-projection matrix
			MapHelper<float4x4> CBConstants(pContext, mVSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
			CBConstants[0] = cam->GetViewProjMatrix();
		}

		// Bind vertex, instance and index buffers
		Uint32   offsets[] = { 0, 0 };
		IBuffer* pBuffs[] = { mCubeVertexBuffer, mInstanceBuffer };
		pContext->SetVertexBuffers(0, _countof(pBuffs), pBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
		pContext->SetIndexBuffer(mCubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		// Set the pipeline state
		pContext->SetPipelineState(mPSO);
		// Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
		// makes sure that resources are transitioned to required states.
		pContext->CommitShaderResources(mSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		DrawIndexedAttribs DrawAttrs;       // This is an indexed draw call
		DrawAttrs.IndexType = VT_UINT16; // Index type
		DrawAttrs.NumIndices = 36;
		DrawAttrs.NumInstances = mAABBs.size(); // The number of instances
		// Verify the state of vertex and index buffers
		DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
		pContext->DrawIndexed(DrawAttrs);
	}

	void DebugCanvas::CreateVertexBuffer(IRenderDevice *pDevice)
	{
		// Layout of this structure matches the one we defined in the pipeline state
		struct Vertex
		{
			float3 pos;
		};

		// Cube vertices

		//      (-1,+1,+1)________________(+1,+1,+1)
		//               /|              /|
		//              / |             / |
		//             /  |            /  |
		//            /   |           /   |
		//(-1,-1,+1) /____|__________/(+1,-1,+1)
		//           |    |__________|____|
		//           |   /(-1,+1,-1) |    /(+1,+1,-1)
		//           |  /            |   /
		//           | /             |  /
		//           |/              | /
		//           /_______________|/
		//        (-1,-1,-1)       (+1,-1,-1)
		//

		// clang-format off
		Vertex CubeVerts[8] =
		{
			{float3(-1,-1,-1)},
			{float3(-1,+1,-1)},
			{float3(+1,+1,-1)},
			{float3(+1,-1,-1)},

			{float3(-1,-1,+1)},
			{float3(-1,+1,+1)},
			{float3(+1,+1,+1)},
			{float3(+1,-1,+1)},
		};
		// clang-format on

		// Create a vertex buffer that stores cube vertices
		BufferDesc VertBuffDesc;
		VertBuffDesc.Name = "Cube vertex buffer";
		VertBuffDesc.Usage = USAGE_IMMUTABLE;
		VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
		VertBuffDesc.uiSizeInBytes = sizeof(CubeVerts);
		BufferData VBData;
		VBData.pData = CubeVerts;
		VBData.DataSize = sizeof(CubeVerts);
		pDevice->CreateBuffer(VertBuffDesc, &VBData, &mCubeVertexBuffer);
	}

	void DebugCanvas::CreateIndexBuffer(IRenderDevice *pDevice)
	{
		// clang-format off
		Uint16 Indices[] =
		{
			2,0,1, 2,3,0,
			4,6,5, 4,7,6,
			0,7,4, 0,3,7,
			1,0,4, 1,4,5,
			1,5,2, 5,6,2,
			3,6,7, 3,2,6
		};
		// clang-format on

		BufferDesc IndBuffDesc;
		IndBuffDesc.Name = "Cube index buffer";
		IndBuffDesc.Usage = USAGE_IMMUTABLE;
		IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
		IndBuffDesc.uiSizeInBytes = sizeof(Indices);
		BufferData IBData;
		IBData.pData = Indices;
		IBData.DataSize = sizeof(Indices);
		pDevice->CreateBuffer(IndBuffDesc, &IBData, &mCubeIndexBuffer);
	}

	void DebugCanvas::CreateInstance(IRenderDevice *pDevice)
	{

	}

	void DebugCanvas::CreatePipelineState(IRenderDevice* pDevice, TEXTURE_FORMAT RTVFormat, TEXTURE_FORMAT DSVFormat, IShaderSourceInputStreamFactory* pShaderSourceFactory)
	{
		GraphicsPipelineStateCreateInfo PSOCreateInfo;
		PipelineStateDesc&              PSODesc = PSOCreateInfo.PSODesc;
		PipelineResourceLayoutDesc&     ResourceLayout = PSODesc.ResourceLayout;
		GraphicsPipelineDesc&           GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

		// This is a graphics pipeline
		PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

		// Pipeline state name is used by the engine to report issues.
		// It is always a good idea to give objects descriptive names.
		PSODesc.Name = "Cube PSO";

		// clang-format off
		// This tutorial will render to a single render target
		GraphicsPipeline.NumRenderTargets = 1;
		// Set render target format which is the format of the swap chain's color buffer
		GraphicsPipeline.RTVFormats[0] = RTVFormat;
		// Set depth buffer format which is the format of the swap chain's back buffer
		GraphicsPipeline.DSVFormat = DSVFormat;
		// Set the desired number of samples
		//GraphicsPipeline.SmplDesc.Count = SampleCount;
		// Primitive topology defines what kind of primitives will be rendered by this pipeline state
		GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		// Cull NONE
		GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
		// Wireframe
		PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FillMode = FILL_MODE_WIREFRAME;
		// Enable depth testing
		GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
		// clang-format on
		ShaderCreateInfo ShaderCI;
		// Tell the system that the shader source code is in HLSL.
		// For OpenGL, the engine will convert this into GLSL under the hood.
		ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

		// OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
		ShaderCI.UseCombinedTextureSamplers = true;

		ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
		// Create a vertex shader
		RefCntAutoPtr<IShader> pVS;
		{
			ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
			ShaderCI.EntryPoint = "main";
			ShaderCI.Desc.Name = "Cube VS";
			ShaderCI.FilePath = "DebugAABBInst.vsh";
			pDevice->CreateShader(ShaderCI, &pVS);
		}

		// Create a pixel shader
		RefCntAutoPtr<IShader> pPS;
		{
			ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
			ShaderCI.EntryPoint = "main";
			ShaderCI.Desc.Name = "Cube PS";
			ShaderCI.FilePath = "DebugAABBInst.psh";
			pDevice->CreateShader(ShaderCI, &pPS);
		}

		// Define vertex shader input layout
   // This tutorial uses two types of input: per-vertex data and per-instance data.
		LayoutElement LayoutElems[] =
		{
			// Per-vertex data - first buffer slot
			// Attribute 0 - vertex position
			LayoutElement{0, 0, 3, VT_FLOAT32, False},
			// Attribute 1 - texture coordinates
			LayoutElement{1, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},

			LayoutElement{2, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
			
		};

		PSOCreateInfo.pVS = pVS;
		PSOCreateInfo.pPS = pPS;

		GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
		GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

		// Define variable type that will be used by default
		ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;	

		pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &mPSO);
	}

}