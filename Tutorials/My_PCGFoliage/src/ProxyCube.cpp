#include "ProxyCube.h"
#include "GraphicsUtilities.h"
#include "FirstPersonCamera.hpp"
#include "MapHelper.hpp"

namespace Diligent
{	
	ProxyCube::ProxyCube()
	{

	}

	ProxyCube::~ProxyCube()
	{

	}


	void ProxyCube::CreateCubeBuffer(IRenderDevice* pDevice)
	{
		//vertex buffer
		// Layout of this structure matches the one we defined in the pipeline state
		struct Vertex
		{
			float3 pos;
			float2 uv;
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
		Vertex CubeVerts[] =
		{
			{float3(-1,-1,-1), float2(0,1)},
			{float3(-1,+1,-1), float2(0,0)},
			{float3(+1,+1,-1), float2(1,0)},
			{float3(+1,-1,-1), float2(1,1)},

			{float3(-1,-1,-1), float2(0,1)},
			{float3(-1,-1,+1), float2(0,0)},
			{float3(+1,-1,+1), float2(1,0)},
			{float3(+1,-1,-1), float2(1,1)},

			{float3(+1,-1,-1), float2(0,1)},
			{float3(+1,-1,+1), float2(1,1)},
			{float3(+1,+1,+1), float2(1,0)},
			{float3(+1,+1,-1), float2(0,0)},

			{float3(+1,+1,-1), float2(0,1)},
			{float3(+1,+1,+1), float2(0,0)},
			{float3(-1,+1,+1), float2(1,0)},
			{float3(-1,+1,-1), float2(1,1)},

			{float3(-1,+1,-1), float2(1,0)},
			{float3(-1,+1,+1), float2(0,0)},
			{float3(-1,-1,+1), float2(0,1)},
			{float3(-1,-1,-1), float2(1,1)},

			{float3(-1,-1,+1), float2(1,1)},
			{float3(+1,-1,+1), float2(0,1)},
			{float3(+1,+1,+1), float2(0,0)},
			{float3(-1,+1,+1), float2(1,0)}
		};
		// clang-format on

		BufferDesc VertBuffDesc;
		VertBuffDesc.Name = "Cube vertex buffer";
		VertBuffDesc.Usage = USAGE_IMMUTABLE;
		VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
		VertBuffDesc.uiSizeInBytes = sizeof(CubeVerts);
		BufferData VBData;
		VBData.pData = CubeVerts;
		VBData.DataSize = sizeof(CubeVerts);

		pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_apVertexBuffer);

		//index buffer
		// clang-format off
		Uint32 Indices[] =
		{
			2,0,1,    2,3,0,
			4,6,5,    4,7,6,
			8,10,9,   8,11,10,
			12,14,13, 12,15,14,
			16,18,17, 16,19,18,
			20,21,22, 20,22,23
		};
		// clang-format on

		BufferDesc IndBuffDesc;
		IndBuffDesc.Name = "ProxyCube index buffer";
		IndBuffDesc.Usage = USAGE_IMMUTABLE;
		IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
		IndBuffDesc.uiSizeInBytes = sizeof(Indices);
		BufferData IBData;
		IBData.pData = Indices;
		IBData.DataSize = sizeof(Indices);
		pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_apIndexBuffer);
	}

	void ProxyCube::CreateInstBuffer(IRenderDevice* pDevice, IDeviceContext *pContext)
	{
		for (int i = 0; i < F_LAYER_NUM; ++i)
		{
			uint32_t inst_count = m_pPlantLayerNum[i];

			BufferDesc InstBuffDesc;
			InstBuffDesc.Name = "Instance data buffer";
			// Use default usage as this buffer will only be updated when grid size changes
			InstBuffDesc.Usage = USAGE_DEFAULT;
			InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
			InstBuffDesc.uiSizeInBytes = sizeof(float4x4) * inst_count;
			pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_apInstanceBuffer[i]);
			PopulateInstanceBuffer(pContext, i, inst_count);
		}
	}

	void ProxyCube::PopulateInstanceBuffer(IDeviceContext *pContext, int LayerIdx, int InstCount)
	{
		std::vector<float4x4> InstTransform(InstCount);

		//uint32_t StartIdx = PCG_PLANT_MAX_POSITION_NUM * LayerIdx;

		for (int i = 0; i < InstCount; ++i)
		{
			float scale = 1.0f / (LayerIdx + 1.0f);
			// Random rotation
			float4x4 rotation = float4x4::RotationX(0.0f) * float4x4::RotationY(0.0f) * float4x4::RotationZ(0.0f);
			float3 pos = float3(m_pPlantPositions[LayerIdx][i].x, m_pPlantPositions[LayerIdx][i].y, m_pPlantPositions[LayerIdx][i].z);
			// Combine rotation, scale and translation
			float4x4 matrix = rotation * float4x4::Scale(scale, scale, scale) * float4x4::Translation(pos);
			InstTransform[i] = matrix;
		}		

		int DataSize = sizeof(float4x4) * InstCount;
		pContext->UpdateBuffer(m_apInstanceBuffer[LayerIdx], 0, DataSize, &InstTransform[0], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	}

	void ProxyCube::InitPSO(IRenderDevice *pDevice, ISwapChain *pSwapChain)
	{
		GraphicsPipelineStateCreateInfo PSOCreateInfo;
		PipelineStateDesc&              PSODesc = PSOCreateInfo.PSODesc;
		PipelineResourceLayoutDesc&     ResourceLayout = PSODesc.ResourceLayout;
		GraphicsPipelineDesc&           GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

		// This is a graphics pipeline
		PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

		// Pipeline state name is used by the engine to report issues.
		// It is always a good idea to give objects descriptive names.
		PSODesc.Name = "ProxyCube Instance PSO";

		// clang-format off
		// This tutorial will render to a single render target
		GraphicsPipeline.NumRenderTargets = 1;
		// Set render target format which is the format of the swap chain's color buffer
		PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = pSwapChain->GetDesc().ColorBufferFormat;
		// Use the depth buffer format from the swap chain
		PSOCreateInfo.GraphicsPipeline.DSVFormat = pSwapChain->GetDesc().DepthBufferFormat;
		// Primitive topology defines what kind of primitives will be rendered by this pipeline state
		GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		// Cull back faces
		GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
		// Enable depth testing
		GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
		// clang-format on
		ShaderCreateInfo ShaderCI;
		// Tell the system that the shader source code is in HLSL.
		// For OpenGL, the engine will convert this into GLSL under the hood.
		ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

		// OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
		ShaderCI.UseCombinedTextureSamplers = true;

		RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
		pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
		ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
		// Create a vertex shader
		RefCntAutoPtr<IShader> pVS;
		{
			ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
			ShaderCI.EntryPoint = "main";
			ShaderCI.Desc.Name = "Instance Cube VS";
			ShaderCI.FilePath = "ProxyCubeInst.vsh";
			pDevice->CreateShader(ShaderCI, &pVS);
		}

		// Create a pixel shader
		RefCntAutoPtr<IShader> pPS;
		{
			ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
			ShaderCI.EntryPoint = "main";
			ShaderCI.Desc.Name = "Instance Cube PS";
			ShaderCI.FilePath = "ProxyCubeInst.psh";
			pDevice->CreateShader(ShaderCI, &pPS);
		}

		// Define vertex shader input layout
		// This tutorial uses two types of input: per-vertex data and per-instance data.
		// clang-format off
		const LayoutElement DefaultLayoutElems[] =
		{
			// Per-vertex data - first buffer slot
			// Attribute 0 - vertex position
			LayoutElement{0, 0, 3, VT_FLOAT32, False},
			// Attribute 1 - texture coordinates
			LayoutElement{1, 0, 2, VT_FLOAT32, False},

			// Per-instance data - second buffer slot
			// We will use four attributes to encode instance-specific 4x4 transformation matrix
			// Attribute 2 - first row
			LayoutElement{2, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
			// Attribute 3 - second row
			LayoutElement{3, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
			// Attribute 4 - third row
			LayoutElement{4, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
			// Attribute 5 - fourth row
			LayoutElement{5, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}
		};
		// clang-format on

		PSOCreateInfo.pVS = pVS;
		PSOCreateInfo.pPS = pPS;

		GraphicsPipeline.InputLayout.LayoutElements = DefaultLayoutElems;
		GraphicsPipeline.InputLayout.NumElements = _countof(DefaultLayoutElems);

		// Define variable type that will be used by default
		ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

		// Shader variables should typically be mutable, which means they are expected
		// to change on a per-instance basis
		// clang-format off
		ShaderResourceVariableDesc Vars[] =
		{
			{SHADER_TYPE_PIXEL, "element_scale", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
		};
		// clang-format on
		ResourceLayout.Variables = Vars;
		ResourceLayout.NumVariables = _countof(Vars);

		// Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
		// clang-format off
		//SamplerDesc SamLinearClampDesc
		//{
		//	FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
		//	TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
		//};
		//ImmutableSamplerDesc ImtblSamplers[] =
		//{
		//	{SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}
		//};
		//// clang-format on
		//ResourceLayout.ImmutableSamplers = ImtblSamplers;
		//ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

		pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_apPSO);

		// Create dynamic uniform buffer that will store our transformation matrix
		// Dynamic buffers can be frequently updated by the CPU
		CreateUniformBuffer(pDevice, sizeof(float4x4), "ProxyCube VS constants CB", &m_apVSConstants);

		// Since we did not explcitly specify the type for 'Constants' variable, default
		// type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
		// never change and are bound directly to the pipeline state object.
		m_apPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_apVSConstants);

		// Since we are using mutable variable, we must create a shader resource binding object
		// http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
		m_apPSO->CreateShaderResourceBinding(&m_SRB, true);
	}

	void ProxyCube::SetFoliagePosData(uint32_t *pPlantLayerNum, std::vector<std::shared_ptr<float4[]>> &pPlantPositions)
	{
		m_pPlantLayerNum = pPlantLayerNum;
		m_pPlantPositions = pPlantPositions;
	}

	void ProxyCube::Render(IDeviceContext *pContext, const FirstPersonCamera *pCam)
	{
		{
			// Map the buffer and write current world-view-projection matrix
			MapHelper<float4x4> CBConstants(pContext, m_apVSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
			CBConstants[0] = pCam->GetViewProjMatrix().Transpose();
		}

		for (int Layer = 0; Layer < F_LAYER_NUM; ++Layer)
		{
			// Bind vertex, instance and index buffers
			Uint32   offsets[] = { 0, 0 };
			IBuffer* pBuffs[] = { m_apVertexBuffer, m_apInstanceBuffer[Layer] };
			pContext->SetVertexBuffers(0, _countof(pBuffs), pBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
			pContext->SetIndexBuffer(m_apIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			// Set the pipeline state
			pContext->SetPipelineState(m_apPSO);
			// Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
			// makes sure that resources are transitioned to required states.
			pContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			DrawIndexedAttribs DrawAttrs;       // This is an indexed draw call
			DrawAttrs.IndexType = VT_UINT32; // Index type
			DrawAttrs.NumIndices = 36;
			DrawAttrs.NumInstances = m_pPlantLayerNum[Layer]; // The number of instances
			// Verify the state of vertex and index buffers
			DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
			pContext->DrawIndexed(DrawAttrs);
		}
	}

}