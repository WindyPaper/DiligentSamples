/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#include "My_Water.hpp"
#include "ShaderMacroHelper.hpp"
#include "MapHelper.hpp"
#include "GroundMesh.h"
#include "DebugCanvas.h"

#include "TextureLoader.h"
#include "TextureUtilities.h"
#include "RenderDevice.h"
#include "Image.h"

#include "imgui.h"
#include "imGuIZMO.h"
#include "ImGuiUtils.hpp"

namespace Diligent
{	
	DebugCanvas gDebugCanvas;

SampleBase* CreateSample()
{
    return new My_Water();
}

void WaterData::Init(IRenderDevice* pDevice)
{
	for (int i = 0; i < NOISE_TEX_NUM; ++i)
	{
		TextureLoadInfo loadInfo;

		std::string FileName = "./WaterNoiseTexture/Noise256_" + std::to_string(i) + ".jpg";
		CreateTextureFromFile(FileName.c_str(), loadInfo, pDevice, &NoiseTextures[i]);
	}
}

Uint8 WaterData::BitReverseValue(const Uint8 FFTN)
{
	//Index 1==0b0001 => 0b1000
	//Index 7==0b0111 => 0b1110
	//etc
	assert(FFTN < 256);

	static unsigned char lookup[16] = {
	0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
	0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf, };

	return (lookup[FFTN & 0b1111] << 4) | lookup[FFTN >> 4];

	// Detailed breakdown of the math
	//  + lookup reverse of bottom nibble
	//  |       + grab bottom nibble
	//  |       |        + move bottom result into top nibble
	//  |       |        |     + combine the bottom and top results 
	//  |       |        |     | + lookup reverse of top nibble
	//  |       |        |     | |       + grab top nibble
	//  V       V        V     V V       V
	// (lookup[n&0b1111] << 4) | lookup[n>>4]
}

void My_Water::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    // Pipeline state object encompasses configuration of all GPU stages

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "Simple triangle PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Use the depth buffer format from the swap chain
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// Wireframe
	PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FillMode		= FILL_MODE_WIREFRAME;

    // No back face culling for this tutorial
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    // Disable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

	// Define vertex shader input layout
	LayoutElement LayoutElems[] =
	{
		// Attribute 0 - vertex position
		LayoutElement{0, 0, 3, VT_FLOAT32, False},
		// Attribute 1 - vertex color
		LayoutElement{1, 0, 4, VT_FLOAT32, False}
	};
	// clang-format on
	PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
	PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

    // clang-format on

    ShaderCreateInfo ShaderCI;
	m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &m_pShaderSourceFactory);
	ShaderCI.pShaderSourceStreamFactory = m_pShaderSourceFactory;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.UseCombinedTextureSamplers = true;
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Terrain vertex shader";
        //ShaderCI.Source          = VSSource;
		ShaderCI.FilePath		 = "cube.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);

		//Create dynamic buffer
		BufferDesc CBDesc;
		CBDesc.Name = "VS Constants CB";
		CBDesc.uiSizeInBytes = sizeof(float4x4);
		CBDesc.Usage = USAGE_DYNAMIC;
		CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
		CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
		m_pDevice->CreateBuffer(CBDesc, nullptr, &m_pVsConstBuf);
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Terrain pixel shader";
        //ShaderCI.Source          = PSSource;
		ShaderCI.FilePath		 = "cube.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    // Finally, create the pipeline state
    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);

	// Since we did not explcitly specify the type for 'Constants' variable, default
	// type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
	// change and are bound directly through the pipeline state object.
	m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pVsConstBuf);

	// Create a shader resource binding object and bind all static resources in it
	m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);

	CreateGridBuffer();

	m_Camera.SetPos(float3(0.0f, 20.0f, -5.f));		
	m_Camera.SetRotationSpeed(0.005f);
	m_Camera.SetMoveSpeed(5.f);
	m_Camera.SetSpeedUpScales(5.f, 10.f);

	m_Camera.SetLookAt(float3(0, 0, 0));

	m_apClipMap.reset(new GroundMesh(LOD_MESH_GRID_SIZE, LOD_COUNT, 0.115f));

	m_apClipMap->InitClipMap(m_pDevice, m_pSwapChain);

	//water
	mWaterData.Init(m_pDevice);
	m_CSGroupSize = 32;
	CreateConstantsBuffer();
	CreateComputePSO();
}

// Render a frame
void My_Water::Render()
{
    // Clear the back buffer
    const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
    // Let the engine perform required state transitions
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	// Bind vertex and index buffers
	Uint32   offset = 0;
	IBuffer* pBuffs[] = { m_TerrainData.pVertexBuf };
	m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
	m_pImmediateContext->SetIndexBuffer(m_TerrainData.pIdxBuf, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	// Set uniform
	{		
		// Map the buffer and write current world-view-projection matrix
		MapHelper<float4x4> CBConstants(m_pImmediateContext, m_pVsConstBuf, MAP_WRITE, MAP_FLAG_DISCARD);		

		*CBConstants = m_Camera.GetViewProjMatrix();
	}

    // Set the pipeline state in the immediate context
    m_pImmediateContext->SetPipelineState(m_pPSO);

	// Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
	// makes sure that resources are transitioned to required states.
	m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	DrawIndexedAttribs drawAttrs;
	drawAttrs.IndexType = VT_UINT32; // Index type
	drawAttrs.NumIndices = m_TerrainData.IdxNum;
	// Verify the state of vertex and index buffers
	drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(drawAttrs);

	m_apClipMap->Render(m_pImmediateContext, m_Camera.GetPos());


	//water
	WaterRender();

	//render debug view
	//gDebugCanvas.Draw(m_pDevice, m_pSwapChain, m_pImmediateContext, m_pShaderSourceFactory, &m_Camera);
}

void My_Water::Update(double CurrTime, double ElapsedTime)
{
	UpdateUI();
    SampleBase::Update(CurrTime, ElapsedTime);

	m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));

	m_apClipMap->Update(&m_Camera);
}

void MakePlane(int rows, int columns, TerrainVertexAttrData *vertices, int *indices)
{
	// Set up vertices
	for (int i = 0, y = 0; y <= rows; y++) {
		for (int x = 0; x <= columns; x++, i++) {
			vertices[i].pos = float3((float)x, 0.0f, (float)y);
		}
	}

	// Set up indices
	for (int ti = 0, vi = 0, y = 0; y < rows; y++, vi++) {
		for (int x = 0; x < columns; x++, ti += 6, vi++) {
			indices[ti] = vi;
			indices[ti + 3] = indices[ti + 2] = vi + 1;
			indices[ti + 4] = indices[ti + 1] = vi + columns + 1;
			indices[ti + 5] = vi + columns + 2;
		}
	}
}

void My_Water::CreateGridBuffer()
{
	int row = 10;
	int column = 10;
	
	const int vertexNum = (column + 1) * (row + 1);
	const int indexNum = column * row * 6; //(row * column) + (column - 1)*(row - 2);

	TerrainVertexAttrData* vertexBuf = new TerrainVertexAttrData[vertexNum];
	int* indexBuf = new int[indexNum];

	MakePlane(row, column, vertexBuf, indexBuf);

	//vertex
	BufferDesc VertBufDesc;
	VertBufDesc.Name = "Terrain Vertex Buffer";
	VertBufDesc.Usage = USAGE_IMMUTABLE;
	VertBufDesc.BindFlags = BIND_VERTEX_BUFFER;
	VertBufDesc.uiSizeInBytes = sizeof(TerrainVertexAttrData) * vertexNum;

	BufferData VBData;
	VBData.pData = vertexBuf;
	VBData.DataSize = sizeof(TerrainVertexAttrData) * vertexNum;
	m_pDevice->CreateBuffer(VertBufDesc, &VBData, &m_TerrainData.pVertexBuf);

	//index
	BufferDesc IdxBufDesc;
	IdxBufDesc.Name = "Terrain Index Buffer";
	IdxBufDesc.Usage = USAGE_IMMUTABLE;
	IdxBufDesc.BindFlags = BIND_INDEX_BUFFER;
	IdxBufDesc.uiSizeInBytes = sizeof(int) * indexNum;

	BufferData IdxData;
	IdxData.pData = indexBuf;
	IdxData.DataSize = sizeof(int) * indexNum;
	m_pDevice->CreateBuffer(IdxBufDesc, &IdxData, &m_TerrainData.pIdxBuf);

	m_TerrainData.VertexNum = vertexNum;
	m_TerrainData.IdxNum = indexNum;

	delete[] vertexBuf;
	delete[] indexBuf;
}

void My_Water::WindowResize(Uint32 Width, Uint32 Height)
{
	float NearPlane = 0.1f;
	float FarPlane = 100000.f;
	float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
	m_Camera.SetProjAttribs(NearPlane, FarPlane, AspectRatio, PI_F / 4.f,
		m_pSwapChain->GetDesc().PreTransform, m_pDevice->GetDeviceCaps().IsGLDevice());
	m_Camera.SetSpeedUpScales(100.0f, 300.0f);
}

void My_Water::UpdateUI()
{
	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Camera Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{		
		float3 CamPos = m_Camera.GetPos();
		ImGui::Text("Cam pos %.2f, %.2f, %.2f", CamPos.x, CamPos.y, CamPos.z);
		float3 CamForward = m_Camera.GetWorldAhead();
		ImGui::Text("Cam Forward %.2f, %.2f, %.2f", CamForward.x, CamForward.y, CamForward.z);
		ImGui::gizmo3D("Cam direction", CamForward, ImGui::GetTextLineHeight() * 10);

		ImGui::gizmo3D("Directional Light", m_LightManager.DirLight.dir, ImGui::GetTextLineHeight() * 10);
	}
	ImGui::End();
}

void My_Water::CreateComputePSO()
{
	CreateH0PSO();
}

void My_Water::CreateConstantsBuffer()
{
	BufferDesc ConstBufferDesc;
	ConstBufferDesc.Name = "Water H0 Constants buffer";
	ConstBufferDesc.Usage = USAGE_DYNAMIC;
	ConstBufferDesc.BindFlags = BIND_UNIFORM_BUFFER;
	ConstBufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
	ConstBufferDesc.uiSizeInBytes = sizeof(WaterFFTH0Uniform);
	m_pDevice->CreateBuffer(ConstBufferDesc, nullptr, &m_apConstants);

	BufferDesc HKTConstBufferDesc;
	HKTConstBufferDesc.Name = "Water HKT Constants buffer";
	HKTConstBufferDesc.Usage = USAGE_DYNAMIC;
	HKTConstBufferDesc.BindFlags = BIND_UNIFORM_BUFFER;
	HKTConstBufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
	HKTConstBufferDesc.uiSizeInBytes = sizeof(WaterFFTHKTUniform);
	m_pDevice->CreateBuffer(HKTConstBufferDesc, nullptr, &m_apHKTConstData);
	
	//h0 h0-1 buffer
	BufferDesc BuffDesc;
	BuffDesc.Name = "H0 H0Minusk buffer";
	BuffDesc.Usage = USAGE_DEFAULT;
	BuffDesc.ElementByteStride = sizeof(float4);
	BuffDesc.Mode = BUFFER_MODE_FORMATTED;
	BuffDesc.uiSizeInBytes = BuffDesc.ElementByteStride * static_cast<Uint32>(WATER_FFT_N * WATER_FFT_N);
	BuffDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_apH0Buffer);
	m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_apH0MinuskBuffer);

	//hkt buffer
	BufferDesc HKTBuffDesc;
	HKTBuffDesc.Name = "HKT buffer";
	HKTBuffDesc.Usage = USAGE_DEFAULT;
	HKTBuffDesc.ElementByteStride = sizeof(float4);
	HKTBuffDesc.Mode = BUFFER_MODE_FORMATTED;
	HKTBuffDesc.uiSizeInBytes = BuffDesc.ElementByteStride * static_cast<Uint32>(WATER_FFT_N * WATER_FFT_N);
	HKTBuffDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	m_pDevice->CreateBuffer(HKTBuffDesc, nullptr, &m_apHKTDX);
	m_pDevice->CreateBuffer(HKTBuffDesc, nullptr, &m_apHKTDY);
	m_pDevice->CreateBuffer(HKTBuffDesc, nullptr, &m_apHKTDZ);
}

void My_Water::WaterRender()
{	
	 //m_apH0ResDataSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Constant")

	//H0
	m_pImmediateContext->SetPipelineState(m_apH0PSO);

	{
		MapHelper<WaterFFTH0Uniform> GPUFFTH0Uniform(m_pImmediateContext, m_apConstants, MAP_WRITE, MAP_FLAG_DISCARD);
		GPUFFTH0Uniform->N_L_Amplitude_Intensity = float4(WATER_FFT_N, 1000, 2, 80);
		GPUFFTH0Uniform->WindDir_LL_Alignment = float4(1.0, 1.0, 0.1, 0.0);
	}

	m_pImmediateContext->CommitShaderResources(m_apH0ResDataSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	DispatchComputeAttribs DispAttr;
	m_pImmediateContext->DispatchCompute(DispAttr);

	//HKT

}

void My_Water::CreateH0PSO()
{
	ShaderCreateInfo ShaderCI;
	m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &m_pShaderSourceFactory);
	ShaderCI.pShaderSourceStreamFactory = m_pShaderSourceFactory;
	// Tell the system that the shader source code is in HLSL.
	// For OpenGL, the engine will convert this into GLSL under the hood.
	ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	// OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
	ShaderCI.UseCombinedTextureSamplers = true;

	ShaderMacroHelper Macros;
	Macros.AddShaderMacro("THREAD_GROUP_SIZE", m_CSGroupSize);
	Macros.Finalize();

	RefCntAutoPtr<IShader> pH0CS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
		ShaderCI.EntryPoint = "main";
		ShaderCI.Desc.Name = "Water H0";
		ShaderCI.FilePath = "water_h0.csh";
		//Macros.AddShaderMacro("UPDATE_SPEED", 1);
		ShaderCI.Macros = Macros;
		m_pDevice->CreateShader(ShaderCI, &pH0CS);
	}

	ComputePipelineStateCreateInfo PSOCreateInfo;
	PipelineStateDesc&             PSODesc = PSOCreateInfo.PSODesc;

	// This is a compute pipeline
	PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

	PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
	// clang-format off
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
	};
	// clang-format on
	PSODesc.ResourceLayout.Variables = Vars;
	PSODesc.ResourceLayout.NumVariables = _countof(Vars);

	PSODesc.Name = "H0 Compute shader";
	PSOCreateInfo.pCS = pH0CS;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apH0PSO);

	IShaderResourceVariable* pConst = m_apH0PSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "Constants");
	if (pConst)
		pConst->Set(m_apConstants);
	m_apH0PSO->CreateShaderResourceBinding(&m_apH0ResDataSRB, true);

	//init texture
	for (int i = 0; i < WaterData::NOISE_TEX_NUM; ++i)
	{
		std::string VarName = "g_NoiseTexture" + std::to_string(i);
		IShaderResourceVariable *pTex = m_apH0ResDataSRB->GetVariableByName(SHADER_TYPE_COMPUTE, VarName.c_str());
		if (pTex)
			pTex->Set(mWaterData.NoiseTextures[i]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	}
	//init data
	{
		/*
		wind.x = 1
		wind.y = 1
		wind.speed = 80
		alignment = 0
		fft.resolution = 256
		fft.amplitude = 2
		fft.L = 1000
		fft.capillarwavesSuppression = 0.1
		*/
		/*MapHelper<WaterFFTH0Uniform> GPUFFTH0Uniform(m_pImmediateContext, m_apConstants, MAP_WRITE, MAP_FLAG_DISCARD);
		GPUFFTH0Uniform->N_L_Amplitude_Intensity = float4(WATER_FFT_N, 1000, 2, 80);
		GPUFFTH0Uniform->WindDir_LL_Alignment = float4(1.0, 1.0, 0.1, 0.0);*/
	}
	//init h0 buffer view
	RefCntAutoPtr<IBufferView> H0UAV, H0MinuskUVA;
	{
		BufferViewDesc ViewDesc;
		ViewDesc.ViewType = BUFFER_VIEW_UNORDERED_ACCESS;
		ViewDesc.Format.ValueType = VT_FLOAT32;
		ViewDesc.Format.NumComponents = 4;
		m_apH0Buffer->CreateView(ViewDesc, &H0UAV);
		m_apH0MinuskBuffer->CreateView(ViewDesc, &H0MinuskUVA);
	}
	IShaderResourceVariable* UAVH0 = m_apH0ResDataSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "buffer_h0k");
	if (UAVH0)
		UAVH0->Set(H0UAV);
	IShaderResourceVariable* UAVH0Minusk = m_apH0ResDataSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "buffer_h0minusk");
	if (UAVH0Minusk)
		UAVH0Minusk->Set(H0MinuskUVA);
}

void My_Water::CreateHKTPSO()
{
	ShaderCreateInfo ShaderCI;
	m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &m_pShaderSourceFactory);
	ShaderCI.pShaderSourceStreamFactory = m_pShaderSourceFactory;
	// Tell the system that the shader source code is in HLSL.
	// For OpenGL, the engine will convert this into GLSL under the hood.
	ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	// OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
	ShaderCI.UseCombinedTextureSamplers = true;

	ShaderMacroHelper Macros;
	Macros.AddShaderMacro("THREAD_GROUP_SIZE", m_CSGroupSize);
	Macros.Finalize();

	RefCntAutoPtr<IShader> HKTShader;
	ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
	ShaderCI.EntryPoint = "main";
	ShaderCI.Desc.Name = "Water HKT";
	ShaderCI.FilePath = "water_hkt.csh";
	//Macros.AddShaderMacro("UPDATE_SPEED", 1);
	ShaderCI.Macros = Macros;
	m_pDevice->CreateShader(ShaderCI, &HKTShader);

	ComputePipelineStateCreateInfo PSOCreateInfo;
	PipelineStateDesc&             PSODesc = PSOCreateInfo.PSODesc;

	// This is a compute pipeline
	PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

	PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
	// clang-format off
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
	};
	// clang-format on
	PSODesc.ResourceLayout.Variables = Vars;
	PSODesc.ResourceLayout.NumVariables = _countof(Vars);

	PSODesc.Name = "HKT Compute shader";
	PSOCreateInfo.pCS = HKTShader;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apHKTPSO);

	IShaderResourceVariable* pConst = m_apHKTPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "Constants");
	if (pConst)
		pConst->Set(m_apConstants);
	m_apHKTPSO->CreateShaderResourceBinding(&m_apHKTDataSRB, true);
}

} // namespace Diligent
