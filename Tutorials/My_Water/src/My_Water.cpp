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

#include <algorithm>

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

#include "RenderProfile.h"

namespace Diligent
{	
	RenderProfileMgr gRenderProfileMgr;

	DebugCanvas gDebugCanvas;

SampleBase* CreateSample()
{
    return new My_Water();
}

void WaterData::Init(IRenderDevice* pDevice)
{
	TextureLoadInfo loadInfo;
	for (int i = 0; i < NOISE_TEX_NUM; ++i)
	{		
		std::string FileName = "./WaterNoiseTexture/Noise256_" + std::to_string(i) + ".jpg";
		CreateTextureFromFile(FileName.c_str(), loadInfo, pDevice, &NoiseTextures[i]);
	}	

	CreateTextureFromFile("./WaterFoamMap/foam_intensity_perlin2_rgb.dds", loadInfo, pDevice, &FoamDiffuseTexture);
}

void WaterData::GenerateBitReversedData(int* OutData)
{
	for (int i = 0; i < WATER_FFT_N; ++i)
	{
		OutData[i] = BitReverseValue(i);
	}
}

Uint8 WaterData::BitReverseValue(const Uint8 FFTN)
{
	//Index 1==0b0001 => 0b1000
	//Index 7==0b0111 => 0b1110
	//etc
	assert(FFTN < 256);

	Uint8 MaxBitCount = (Uint8)std::log2(255) + 1;
	Uint8 BitCount = (Uint8)std::log2(WATER_FFT_N);
	Uint8 ShiftBitCount = MaxBitCount - BitCount;

	static unsigned char lookup[16] = {
	0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
	0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf, };

	Uint8 ReversedValue = (lookup[FFTN & 0b1111] << 4) | lookup[FFTN >> 4];
	ReversedValue = ReversedValue >> ShiftBitCount;

	return ReversedValue;


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

	m_ShaderUniformDataMgr.CreateGPUBuffer(m_pDevice);

	m_Camera.SetPos(float3(0.0f, 20.0f, -5.f));		
	m_Camera.SetRotationSpeed(0.005f);
	m_Camera.SetMoveSpeed(5.f);
	m_Camera.SetSpeedUpScales(5.f, 10.f);

	m_Camera.SetLookAt(float3(0, 0, 0));

	m_apClipMap.reset(new GroundMesh(LOD_MESH_GRID_SIZE, LOD_COUNT, 0.115f));

	m_apClipMap->InitClipMap(m_pDevice, m_pSwapChain, &m_ShaderUniformDataMgr);

	//profile
	gRenderProfileMgr.Initialize(m_pDevice, m_pImmediateContext);	

	//water
	m_Choppy = true;
	mWaterTimer.Restart();
	mWaterData.Init(m_pDevice);
	m_CSGroupSize = 32;
	m_Log2_N = std::log2(WATER_FFT_N);
	CreateConstantsBuffer();
	CreateComputePSO();
}

// Render a frame
void My_Water::Render()
{
	//water
	{
		GPUProfileScope gpuscope(&gRenderProfileMgr, "WaterFFT");
		WaterRender();
	}	

	//water mesh
	{
		GPUProfileScope gpuscope(&gRenderProfileMgr, "Water");

		// Clear the back buffer
		const float ClearColor[] = { 0.350f, 0.350f, 0.350f, 1.0f };
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
		
		WaterRenderData WRenderData;
		WRenderData.L_RepeatScale_NormalIntensity_N = float4(m_WaterRenderParam.size_L, m_WaterRenderParam.RepeatScale, m_WaterRenderParam.BaseNormalIntensity, WATER_FFT_N);
		WRenderData.pHeightMap = m_apFFTDisplacementTexture;
		WRenderData.pFoamDiffuseMap = mWaterData.FoamDiffuseTexture;
		WRenderData.pFoamMaskMap = m_apFFTFoamTexture;
		m_apClipMap->Render(m_pImmediateContext, m_Camera.GetPos(), WRenderData);
	}

	//render debug view
	//gDebugCanvas.Draw(m_pDevice, m_pSwapChain, m_pImmediateContext, m_pShaderSourceFactory, &m_Camera);
}

void My_Water::Update(double CurrTime, double ElapsedTime)
{
	//profile
	UpdateProfileData();

	UpdateUI();
    SampleBase::Update(CurrTime, ElapsedTime);

	m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));

	m_apClipMap->Update(&m_Camera);

	//update const data
	{
		MapHelper<XLightStructure> ShaderConstUniform(m_pImmediateContext, m_ShaderUniformDataMgr.GetLightStructure(), MAP_WRITE, MAP_FLAG_DISCARD);
		ShaderConstUniform->Direction = float4(-m_LightManager.DirLight.dir, m_LightManager.DirLight.intensity);
		ShaderConstUniform->AmbientLight = float4(1, 1, 1, 1);
	}
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
		ImGui::SliderFloat("DL Intensity", &m_LightManager.DirLight.intensity, 0.1, 10);		
	}
	ImGui::End();

	if (ImGui::Begin("Ocean params", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::SliderFloat("Amplitude", &m_WaterRenderParam.Amplitude, 0.1, 10);
		ImGui::SliderFloat("L", &m_WaterRenderParam.size_L, 100, 10000);
		ImGui::gizmo3D("Wind Direction Light", m_WaterRenderParam.WindDir, ImGui::GetTextLineHeight() * 10);
		ImGui::SliderFloat("WindIntensity", &m_WaterRenderParam.WindIntensity, 0.01, 100);
		ImGui::SliderFloat("ChoppyScale", &m_WaterRenderParam.ChoppyScale, 0.01, 10);		
		ImGui::SliderFloat("RepeatScale", &m_WaterRenderParam.RepeatScale, 1.0, 50.0);
		ImGui::SliderFloat("BaseNormal Intensity", &m_WaterRenderParam.BaseNormalIntensity, 0.1, 10.0);
	}	
	ImGui::End();
}

void My_Water::CreateComputePSO()
{
	CreateH0PSO();	

	CreateFFTRowPSO();
	CreateFFTColumnPSO();
	CreateFoamPSO();
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
	
	//h0 h0-1 buffer	
	TextureDesc TexDesc;
	TexDesc.Type = RESOURCE_DIM_TEX_2D;
	TexDesc.Width = WATER_FFT_N;
	TexDesc.Height = WATER_FFT_N;
	TexDesc.MipLevels = 1;
	TexDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
	TexDesc.Usage = USAGE_DEFAULT;
	TexDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	m_pDevice->CreateTexture(TexDesc, nullptr, &m_apH0Buffer);
	m_pDevice->CreateTexture(TexDesc, nullptr, &m_apH0MinuskBuffer);
	
	//FFT Row
	TextureDesc HtTexDesc;
	HtTexDesc.Type = RESOURCE_DIM_TEX_2D;
	HtTexDesc.Width = WATER_FFT_N;
	HtTexDesc.Height = WATER_FFT_N / 2 + 1;
	HtTexDesc.MipLevels = 1;
	HtTexDesc.Format = TEX_FORMAT_RG32_FLOAT;
	HtTexDesc.Usage = USAGE_DYNAMIC;
	HtTexDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	m_pDevice->CreateTexture(HtTexDesc, nullptr, &m_apFFTHtTex);

	TextureDesc DtTexDesc;
	DtTexDesc.Type = RESOURCE_DIM_TEX_2D;
	DtTexDesc.Width = WATER_FFT_N;
	DtTexDesc.Height = WATER_FFT_N / 2 + 1;
	DtTexDesc.MipLevels = 1;
	DtTexDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
	DtTexDesc.Usage = USAGE_DYNAMIC;
	DtTexDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	m_pDevice->CreateTexture(DtTexDesc, nullptr, &m_apFFTDtTex);

	BufferDesc FastFFTData;
	FastFFTData.Name = "Water Fast FFT Data";
	FastFFTData.Usage = USAGE_DYNAMIC;
	FastFFTData.BindFlags = BIND_UNIFORM_BUFFER;
	FastFFTData.CPUAccessFlags = CPU_ACCESS_WRITE;
	FastFFTData.uiSizeInBytes = sizeof(float4);
	m_pDevice->CreateBuffer(FastFFTData, nullptr, &m_apFFTRowData);

	//FFT column
	TextureDesc FFTDisplacmentTexDesc;
	FFTDisplacmentTexDesc.Type = RESOURCE_DIM_TEX_2D;
	FFTDisplacmentTexDesc.Width = WATER_FFT_N;
	FFTDisplacmentTexDesc.Height = WATER_FFT_N;
	FFTDisplacmentTexDesc.MipLevels = 1;
	FFTDisplacmentTexDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
	FFTDisplacmentTexDesc.Usage = USAGE_DYNAMIC;
	FFTDisplacmentTexDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	m_pDevice->CreateTexture(FFTDisplacmentTexDesc, nullptr, &m_apFFTDisplacementTexture);
	m_pDevice->CreateBuffer(FastFFTData, nullptr, &m_apFFTColumnData);

	//FFT Foam
	TextureDesc FFTFoamTexDesc;
	FFTFoamTexDesc.Type = RESOURCE_DIM_TEX_2D;
	FFTFoamTexDesc.Width = WATER_FFT_N;
	FFTFoamTexDesc.Height = WATER_FFT_N;
	FFTFoamTexDesc.MipLevels = 1;
	FFTFoamTexDesc.Format = TEX_FORMAT_R32_FLOAT;
	FFTFoamTexDesc.Usage = USAGE_DYNAMIC;
	FFTFoamTexDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	m_pDevice->CreateTexture(FFTFoamTexDesc, nullptr, &m_apFFTFoamTexture);
	m_pDevice->CreateBuffer(FastFFTData, nullptr, &m_apFFTFoamData);
}

void My_Water::WaterRender()
{	
	 //m_apH0ResDataSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Constant")
	DispatchComputeAttribs DispAttr;

	int DispatchXNum = std::max(1, WATER_FFT_N / m_CSGroupSize);
	int DispatchYNum = std::max(1, WATER_FFT_N / m_CSGroupSize);
	DispAttr.ThreadGroupCountX = DispatchXNum;
	DispAttr.ThreadGroupCountY = DispatchYNum;

	//H0 (Not necessary to render every frame)
	m_pImmediateContext->SetPipelineState(m_apH0PSO);
	{
		MapHelper<WaterFFTH0Uniform> GPUFFTH0Uniform(m_pImmediateContext, m_apConstants, MAP_WRITE, MAP_FLAG_DISCARD);
		GPUFFTH0Uniform->N_L_Amplitude_Intensity = float4(WATER_FFT_N, m_WaterRenderParam.size_L, mWaterTimer.GetWaterTime(), m_WaterRenderParam.WindIntensity);
		float2 NormalizeWindDir = normalize(float2(m_WaterRenderParam.WindDir.x, m_WaterRenderParam.WindDir.z));
		GPUFFTH0Uniform->WindDir_LL_Alignment = float4(NormalizeWindDir.x, NormalizeWindDir.y, 0.7, 0.0);
	}
	m_pImmediateContext->CommitShaderResources(m_apH0ResDataSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);	
	m_pImmediateContext->DispatchCompute(DispAttr);


	//FFT Row
	m_pImmediateContext->SetPipelineState(m_apFFTRowPSO);
	{
		MapHelper<WaterFFTRowUniform> GPUFFTRowUniform(m_pImmediateContext, m_apFFTRowData, MAP_WRITE, MAP_FLAG_DISCARD);
		GPUFFTRowUniform->N_ChoppyScale_NBitNum_Time = float4(WATER_FFT_N, m_WaterRenderParam.size_L, 32 - m_Log2_N, m_WaterRenderParam.ChoppyScale);
	}
	m_pImmediateContext->CommitShaderResources(m_apFFTRowSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	DispatchComputeAttribs FFTRowDisp(1, WATER_FFT_N / 2 + 1);
	m_pImmediateContext->DispatchCompute(FFTRowDisp);

	//FFT Column
	m_pImmediateContext->SetPipelineState(m_apFFTColumnPSO);
	{
		MapHelper<WaterFFTRowUniform> GPUFFTColumnUniform(m_pImmediateContext, m_apFFTColumnData, MAP_WRITE, MAP_FLAG_DISCARD);
		GPUFFTColumnUniform->N_ChoppyScale_NBitNum_Time = float4(WATER_FFT_N, m_WaterRenderParam.ChoppyScale, 32 - m_Log2_N, mWaterTimer.GetWaterTime());
	}
	m_pImmediateContext->CommitShaderResources(m_apFFTColumnSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	DispatchComputeAttribs FFTColumnDisp(1, WATER_FFT_N);
	m_pImmediateContext->DispatchCompute(FFTColumnDisp);	

	//FFT Foam
	m_pImmediateContext->SetPipelineState(m_apFFTFoamPSO);
	m_pImmediateContext->CommitShaderResources(m_apFFTFoamSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	DispatchComputeAttribs FFTFoamDisp(1, WATER_FFT_N);
	m_pImmediateContext->DispatchCompute(FFTFoamDisp);
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
	IShaderResourceVariable* UAVH0 = m_apH0ResDataSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "buffer_h0k");
	if (UAVH0)
		UAVH0->Set(m_apH0Buffer->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
	IShaderResourceVariable* UAVH0Minusk = m_apH0ResDataSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "buffer_h0minusk");
	if (UAVH0Minusk)
		UAVH0Minusk->Set(m_apH0MinuskBuffer->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
}

void My_Water::ConvertToTextureView(IBuffer* pData, int width, int height, int Stride, ITexture **pRetTex)
{
	TextureSubResData MipData;
	MipData.pSrcBuffer = pData;
	MipData.Stride = Stride;

	TextureData TexData;
	TexData.NumSubresources = 1;
	TexData.pSubResources = &MipData;

	TextureDesc TexDesc;
	TexDesc.Type = RESOURCE_DIM_TEX_2D;
	TexDesc.Width = width;
	TexDesc.Height = height;
	TexDesc.MipLevels = 1;
	TexDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
	TexDesc.Usage = USAGE_STAGING;
	TexDesc.BindFlags = BIND_SHADER_RESOURCE;
	
	m_pDevice->CreateTexture(TexDesc, &TexData, pRetTex);
}

void My_Water::UpdateProfileData()
{
	ProfilerTask* pCPUData = nullptr;
	ProfilerTask* pGPUData = nullptr;

	int CPUDataSize = 0;
	int GPUDataSize = 0;

	gRenderProfileMgr.GetCPUProfileData(&pCPUData, CPUDataSize);
	gRenderProfileMgr.GetGPUProfileData(&pGPUData, GPUDataSize);

	mProfilersWindow.gpuGraph.LoadFrameData(pGPUData, GPUDataSize);

	mProfilersWindow.Render();

	gRenderProfileMgr.CleanProfileTask();
}

void My_Water::CreateFFTRowPSO()
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
	Macros.AddShaderMacro("THREAD_GROUP_SIZE", WATER_FFT_N);
	Macros.Finalize();

	RefCntAutoPtr<IShader> pFFTRowCS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
		ShaderCI.EntryPoint = "main";
		ShaderCI.Desc.Name = "FastFFT Row";
		ShaderCI.FilePath = "water_fft_row.csh";
		ShaderCI.Macros = Macros;
		m_pDevice->CreateShader(ShaderCI, &pFFTRowCS);
	}

	ComputePipelineStateCreateInfo PSOCreateInfo;
	PipelineStateDesc&             PSODesc = PSOCreateInfo.PSODesc;

	// This is a compute pipeline
	PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

	PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
	// clang-format off
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},		
		{SHADER_TYPE_COMPUTE, "HtOutput", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "DtOutput", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "H0", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
	};
	// clang-format on
	PSODesc.ResourceLayout.Variables = Vars;
	PSODesc.ResourceLayout.NumVariables = _countof(Vars);

	PSODesc.Name = "FFT Row Compute shader";
	PSOCreateInfo.pCS = pFFTRowCS;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apFFTRowPSO);

	IShaderResourceVariable* pConst = m_apFFTRowPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "Constants");
	if (pConst)
		pConst->Set(m_apFFTRowData);
	m_apFFTRowPSO->CreateShaderResourceBinding(&m_apFFTRowSRB, true);

	IShaderResourceVariable* pHtOutput = m_apFFTRowSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "HtOutput");
	if (pHtOutput)
		pHtOutput->Set(m_apFFTHtTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
	IShaderResourceVariable* pDtOutput = m_apFFTRowSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "DtOutput");
	if (pDtOutput)
		pDtOutput->Set(m_apFFTDtTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
	IShaderResourceVariable* pH0Input= m_apFFTRowSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "H0");
	if (pH0Input)
		pH0Input->Set(m_apH0Buffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
}

void My_Water::CreateFFTColumnPSO()
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
	Macros.AddShaderMacro("THREAD_GROUP_SIZE", WATER_FFT_N);
	Macros.Finalize();

	RefCntAutoPtr<IShader> pFFTColumnCS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
		ShaderCI.EntryPoint = "main";
		ShaderCI.Desc.Name = "FastFFT Column";
		ShaderCI.FilePath = "water_fft_column.csh";
		ShaderCI.Macros = Macros;
		m_pDevice->CreateShader(ShaderCI, &pFFTColumnCS);
	}

	ComputePipelineStateCreateInfo PSOCreateInfo;
	PipelineStateDesc&             PSODesc = PSOCreateInfo.PSODesc;

	// This is a compute pipeline
	PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

	PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
	// clang-format off
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
		{SHADER_TYPE_COMPUTE, "HtInput", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "DtInput", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "displacement", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
	};
	// clang-format on
	PSODesc.ResourceLayout.Variables = Vars;
	PSODesc.ResourceLayout.NumVariables = _countof(Vars);

	PSODesc.Name = "FFT Column Compute shader";
	PSOCreateInfo.pCS = pFFTColumnCS;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apFFTColumnPSO);

	IShaderResourceVariable* pConst = m_apFFTColumnPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "Constants");
	if (pConst)
		pConst->Set(m_apFFTColumnData);
	m_apFFTColumnPSO->CreateShaderResourceBinding(&m_apFFTColumnSRB, true);

	IShaderResourceVariable* pHtInput = m_apFFTColumnSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "HtInput");
	if (pHtInput)
		pHtInput->Set(m_apFFTHtTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	IShaderResourceVariable* pDtInput = m_apFFTColumnSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "DtInput");
	if (pDtInput)
		pDtInput->Set(m_apFFTDtTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	IShaderResourceVariable* pDisplaceTex = m_apFFTColumnSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "displacement");
	if (pDisplaceTex)
		pDisplaceTex->Set(m_apFFTDisplacementTexture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
}

void My_Water::CreateFoamPSO()
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
	Macros.AddShaderMacro("THREAD_GROUP_SIZE", WATER_FFT_N);
	Macros.Finalize();

	RefCntAutoPtr<IShader> pFFTFoamCS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
		ShaderCI.EntryPoint = "main";
		ShaderCI.Desc.Name = "Generate Foam";
		ShaderCI.FilePath = "water_generate_foam.csh";
		ShaderCI.Macros = Macros;
		m_pDevice->CreateShader(ShaderCI, &pFFTFoamCS);
	}

	ComputePipelineStateCreateInfo PSOCreateInfo;
	PipelineStateDesc& PSODesc = PSOCreateInfo.PSODesc;

	// This is a compute pipeline
	PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

	PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
	// clang-format off
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},	
		{SHADER_TYPE_COMPUTE, "FoamTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
		{SHADER_TYPE_COMPUTE, "g_DisplaccementTex", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
	};
	// clang-format on
	PSODesc.ResourceLayout.Variables = Vars;
	PSODesc.ResourceLayout.NumVariables = _countof(Vars);

	// clang-format off
	// Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
	SamplerDesc SamLinearClampDesc
	{
		FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
		TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
	};
	ImmutableSamplerDesc ImtblSamplers[] =
	{
		{SHADER_TYPE_COMPUTE, "g_DisplaccementTex", SamLinearClampDesc}
	};
	// clang-format on
	PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;
	PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

	PSODesc.Name = "FFT Foam Compute shader";
	PSOCreateInfo.pCS = pFFTFoamCS;
	m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apFFTFoamPSO);

	IShaderResourceVariable* pConst = m_apFFTFoamPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "Constants");
	if (pConst)
		pConst->Set(m_apFFTFoamData);
	m_apFFTFoamPSO->CreateShaderResourceBinding(&m_apFFTFoamSRB, true);
	
	IShaderResourceVariable* pDisplaceTex = m_apFFTFoamSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_DisplaccementTex");
	if (pDisplaceTex)
		pDisplaceTex->Set(m_apFFTDisplacementTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	IShaderResourceVariable* pFoamTex = m_apFFTFoamSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "FoamTexture");
	if (pFoamTex)
		pFoamTex->Set(m_apFFTFoamTexture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
}

WaterTimer::WaterTimer()
{
	mSpeed = 1.0f;
	mTCount = 0.0f;

	Restart();
}

void WaterTimer::Restart()
{
	using namespace std::chrono;
	mt = high_resolution_clock().now();
}

float WaterTimer::GetWaterTime()
{
	using namespace std::chrono;

	std::chrono::high_resolution_clock::time_point CurrTime = high_resolution_clock::now();
	auto time_span = duration_cast<duration<float>>(CurrTime - mt);
	mt = CurrTime;
	mTCount += time_span.count() * mSpeed;
	//std::cout << mTCount << std::endl;
	return mTCount;
}

} // namespace Diligent
