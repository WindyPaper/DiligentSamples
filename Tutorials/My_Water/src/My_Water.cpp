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
#include "GraphicsUtilities.h"
#include "RenderDevice.h"
#include "Image.h"

#include "imgui.h"
#include "imGuIZMO.h"
#include "ImGuiUtils.hpp"

#include "RenderProfile.h"

#include "OceanWave.h"
#include "ReflectionProbe.h"
#include "CommonlyUsedStates.h"

namespace Diligent
{	
	RenderProfileMgr gRenderProfileMgr;

	DebugCanvas gDebugCanvas;

SampleBase* CreateSample()
{
    return new My_Water();
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
	PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_R11G11B10_FLOAT;// m_pSwapChain->GetDesc().ColorBufferFormat;
    // Use the depth buffer format from the swap chain
	PSOCreateInfo.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;// m_pSwapChain->GetDesc().DepthBufferFormat;
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
		ShaderCI.FilePath		 = "assets/cube.vsh";
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
		ShaderCI.FilePath		 = "assets/cube.psh";
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

	m_Camera.SetPos(float3(0.0f, 4.0f, -5.f));		
	m_Camera.SetRotationSpeed(0.005f);
	m_Camera.SetMoveSpeed(5.f);
	m_Camera.SetSpeedUpScales(5.f, 10.f);

	m_Camera.SetLookAt(float3(0.0, 0.0, -1000.0));

	m_apClipMap.reset(new GroundMesh(LOD_MESH_GRID_SIZE, LOD_COUNT, 0.115f));

	m_apClipMap->InitClipMap(m_pDevice, m_pSwapChain, &m_ShaderUniformDataMgr);

	//profile
	gRenderProfileMgr.Initialize(m_pDevice, m_pImmediateContext);	

	//Ocean
	m_Choppy = true;
	mWaterTimer.Restart();
	m_CSGroupSize = 32;
	m_Log2_N = std::log2(WATER_FFT_N);
	m_LastTimerCount = mWaterTimer.GetWaterTime();

	m_pOceanWave = new OceanWave(WATER_FFT_N, m_pDevice, m_pShaderSourceFactory);
	m_pOceanWave->Init(WATER_FFT_N);
	m_WaveSwellSetting[0] = new WaveDisplaySetting({1.0f, 1.0f, -30.0f, 100000.0f, 1.0f, 0.198f, 3.3f, 0.01f, 9.8f});
	m_WaveSwellSetting[1] = new WaveDisplaySetting({0.555f, 1.0f, 0.0f, 300000.0f, 1.0f, 1.0f, 5.82f, 0.01f, 9.8f});

	//sky
	const auto& SCDesc = m_pSwapChain->GetDesc();
	m_apSkyScattering.reset(new EpipolarLightScattering(m_pDevice, m_pImmediateContext, SCDesc.ColorBufferFormat, SCDesc.DepthBufferFormat, TEX_FORMAT_R11G11B10_FLOAT, m_pShaderSourceFactory));
	m_apSkyScatteringCube.reset(new EpipolarLightScattering(m_pDevice, m_pImmediateContext, TEX_FORMAT_RGBA32_FLOAT, TEX_FORMAT_D32_FLOAT, TEX_FORMAT_R11G11B10_FLOAT, m_pShaderSourceFactory));

	m_OceanMaterialParams.OceanColor = float4(0.002f, 0.026f, 0.044f, 1.0f);
	m_OceanMaterialParams.SSSColor = float4(0.154f, 0.885f, 0.99f, 1.0f);
	m_OceanMaterialParams.SSSStrength = 0.1f;
	m_OceanMaterialParams.SSSScale = 4.8f;
	m_OceanMaterialParams.SSSBase = -0.1;
	m_OceanMaterialParams.LodScale = 7.5f;
	m_OceanMaterialParams.MaxGloss = 0.0f;
	m_OceanMaterialParams.Roughness = 0.0f;
	m_OceanMaterialParams.RoughnessScale = 0.1f;
	m_OceanMaterialParams.FoamColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
	m_OceanMaterialParams.FoamBiasLod0 = 0.84f;
	m_OceanMaterialParams.FoamBiasLod1 = 1.83f;
	m_OceanMaterialParams.FoamBiasLod2 = 2.52f;
	m_OceanMaterialParams.FoamScale = 3.5f;
	m_OceanMaterialParams.ContactFoam = 0.0f;

	//cubemap
	m_pReflectionProbe = new ReflectionProbe(float3(0.0f, 5000.0f, 0.0f));
	CreateGPUTexture();
}

// Render a frame
void My_Water::Render()
{
	//cube map
	{
		CPUAndGPUProfileScope scope(&gRenderProfileMgr, "Gen CubeMap", Colors::emerald);
		CubeMapRender();
	}

	//set rt for sky atmosphere 
	const float Zero[] = { 0.f, 0.f, 0.f, 0.f };
	auto* pRTV = m_pOffscreenColorBuffer->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
	auto* pDSV = m_pOffscreenDepthBuffer->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
	m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	m_pImmediateContext->ClearRenderTarget(pRTV, Zero, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	//water
	{
		GPUProfileScope gpuscope(&gRenderProfileMgr, "WaterFFT", Colors::peterRiver);
		WaterRender();
	}	

	//water mesh
	{
		CPUAndGPUProfileScope scope(&gRenderProfileMgr, "Water", Colors::alizarin);

		// Bind vertex and index buffers
		//Uint32   offset = 0;
		//IBuffer* pBuffs[] = { m_TerrainData.pVertexBuf };
		//m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
		//m_pImmediateContext->SetIndexBuffer(m_TerrainData.pIdxBuf, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		//// Set uniform
		//{
		//	// Map the buffer and write current world-view-projection matrix
		//	MapHelper<float4x4> CBConstants(m_pImmediateContext, m_pVsConstBuf, MAP_WRITE, MAP_FLAG_DISCARD);

		//	*CBConstants = m_Camera.GetViewProjMatrix();
		//}

		//// Set the pipeline state in the immediate context
		//m_pImmediateContext->SetPipelineState(m_pPSO);

		//// Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
		//// makes sure that resources are transitioned to required states.
		//m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		//DrawIndexedAttribs drawAttrs;
		//drawAttrs.IndexType = VT_UINT32; // Index type
		//drawAttrs.NumIndices = m_TerrainData.IdxNum;
		//// Verify the state of vertex and index buffers
		//drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
		//m_pImmediateContext->DrawIndexed(drawAttrs);
		
		WaterRenderData WRenderData;
		WRenderData.L_RepeatScale_NormalIntensity_N = float4(m_WaterRenderParam.size_L, m_WaterRenderParam.RepeatScale, m_WaterRenderParam.BaseNormalIntensity, WATER_FFT_N);
		//WRenderData.pHeightMap = m_apFFTDisplacementTexture;
		//WRenderData.pFoamDiffuseMap = mWaterData.FoamDiffuseTexture;
		//WRenderData.pFoamMaskMap = m_apFFTFoamTexture;
		WRenderData.pOceanWave = m_pOceanWave;
		WRenderData.pDiffIrradianceMap = m_pIrradianceCubeSRV->GetTexture();
		WRenderData.pIBLSPecMap = m_pPrefilteredEnvMapSRV->GetTexture();
		WRenderData.OceanRMatParams = m_OceanMaterialParams;
		m_apClipMap->Render(m_pImmediateContext, m_Camera.GetPos(), WRenderData);
	}

	//render debug view
	//gDebugCanvas.Draw(m_pDevice, m_pSwapChain, m_pImmediateContext, m_pShaderSourceFactory, &m_Camera);

	//atmosphere sky
	{
		CPUAndGPUProfileScope scope(&gRenderProfileMgr, "Atmosphere", Colors::amethyst);
		AtmosphereRender(&m_Camera, m_pSwapChain->GetCurrentBackBufferRTV(), m_pSwapChain->GetDepthBufferDSV(), m_apSkyScattering.get());
	}
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
	m_Camera.SetProjAttribs(NearPlane, FarPlane, AspectRatio, PI_F / 2.f,
		m_pSwapChain->GetDesc().PreTransform, m_pDevice->GetDeviceCaps().IsGLDevice());
	m_Camera.SetSpeedUpScales(100.0f, 300.0f);

	//m_apSkyScattering->OnWindowResize(m_pDevice, Width, Height);
	// Flush is required because Intel driver does not release resources until
	// command buffer is flushed. When window is resized, WindowResize() is called for
	// every intermediate window size, and light scattering object creates resources
	// for the new size. This resources are then released by the light scattering object, but
	// not by Intel driver, which results in memory exhaustion.
	m_pImmediateContext->Flush();

	m_pOffscreenColorBuffer.Release();
	m_pOffscreenDepthBuffer.Release();

	TextureDesc ColorBuffDesc;
	ColorBuffDesc.Name = "Offscreen color buffer";
	ColorBuffDesc.Type = RESOURCE_DIM_TEX_2D;
	ColorBuffDesc.Width = Width;
	ColorBuffDesc.Height = Height;
	ColorBuffDesc.MipLevels = 1;
	ColorBuffDesc.Format = TEX_FORMAT_R11G11B10_FLOAT;
	ColorBuffDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
	m_pDevice->CreateTexture(ColorBuffDesc, nullptr, &m_pOffscreenColorBuffer);

	TextureDesc DepthBuffDesc = ColorBuffDesc;
	DepthBuffDesc.Name = "Offscreen depth buffer";
	DepthBuffDesc.Format = TEX_FORMAT_D32_FLOAT;
	DepthBuffDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
	m_pDevice->CreateTexture(DepthBuffDesc, nullptr, &m_pOffscreenDepthBuffer);
}

void My_Water::UpdateUI()
{
	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Camera Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{		
		float3 CamPos = m_Camera.GetPos();
		ImGui::Text("Cam pos %.2f, %.2f, %.2f", CamPos.x, CamPos.y, CamPos.z);
		/*float3 CamForward = m_Camera.GetWorldAhead();
		ImGui::Text("Cam Forward %.2f, %.2f, %.2f", CamForward.x, CamForward.y, CamForward.z);
		ImGui::gizmo3D("Cam direction", CamForward, ImGui::GetTextLineHeight() * 10);*/

		ImGui::gizmo3D("Directional Light", m_LightManager.DirLight.dir, ImGui::GetTextLineHeight() * 10);	
		ImGui::SliderFloat("DL Intensity", &m_LightManager.DirLight.intensity, 0.1f, 10.0f);

		if (ImGui::InputFloat("Aerosol Density", &m_PPAttribs.fAerosolDensityScale, 0.1f, 0.25f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
			m_PPAttribs.fAerosolDensityScale = clamp(m_PPAttribs.fAerosolDensityScale, 0.1f, 10.0f);

		if (ImGui::InputFloat("Aerosol Absorption", &m_PPAttribs.fAerosolAbsorbtionScale, 0.1f, 0.25f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
			m_PPAttribs.fAerosolAbsorbtionScale = clamp(m_PPAttribs.fAerosolAbsorbtionScale, 0.0f, 10.0f);
	}
	ImGui::End();

	auto UISwellSettingFunc = [](WaveDisplaySetting *pSetting)
	{
		ImGui::SliderFloat("Scale", &pSetting->scale, 0.01f, 1.0f);
		ImGui::SliderFloat("WindSpeed", &pSetting->WindSpeed, 0.01f, 10.0f);
		ImGui::SliderFloat("WindDirectionalAngle", &pSetting->WindDirectionalAngle, 0.01f, 360.0f);		
		ImGui::SliderFloat("Fetch", &pSetting->fetch, 0.01f, 1000000.0f);
		ImGui::SliderFloat("SpreadBlend", &pSetting->SpreadBlend, 0.01f, 1.0f);
		ImGui::SliderFloat("Swell", &pSetting->swell, 0.01f, 1.0f);
		ImGui::SliderFloat("PeakEnhancement", &pSetting->PeakEnhancement, 0.01f, 120.0f);
		ImGui::SliderFloat("ShortWavesFade", &pSetting->ShortWavesFade, 0.01f, 1.0f);
	};

	if (ImGui::Begin("Ocean params", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		//if (ImGui::Button("LocalSpectrumSetting"))
		if (ImGui::Begin("local spectrum params", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			UISwellSettingFunc(m_WaveSwellSetting[0]);
		}
		ImGui::End();

		//if (ImGui::Button("SwellSpectrumSetting"))
		if (ImGui::Begin("swell spectrum params", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			UISwellSettingFunc(m_WaveSwellSetting[1]);
		}
		ImGui::End();

		if (ImGui::Begin("ocean material params", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::ColorEdit4("OceanColor", &m_OceanMaterialParams.OceanColor[0]);
			ImGui::ColorEdit4("SSSColor", &m_OceanMaterialParams.SSSColor[0]);

			ImGui::SliderFloat("SSSStrength", &m_OceanMaterialParams.SSSStrength, 0.01f, 1.0f);
			ImGui::SliderFloat("SSSScale", &m_OceanMaterialParams.SSSScale, 0.01f, 50.0f);
			ImGui::SliderFloat("SSSBase", &m_OceanMaterialParams.SSSBase, -5.0f, 1.0f);
			ImGui::SliderFloat("LodScale", &m_OceanMaterialParams.LodScale, 0.01f, 100.0f);

			ImGui::SliderFloat("MaxGloss", &m_OceanMaterialParams.MaxGloss, 0.01f, 1.0f);
			ImGui::SliderFloat("Roughness", &m_OceanMaterialParams.Roughness, 0.01f, 1.0f);
			ImGui::SliderFloat("RoughnessScale", &m_OceanMaterialParams.RoughnessScale, 0.0f, 0.5f);
			ImGui::SliderFloat("ContactFoam", &m_OceanMaterialParams.ContactFoam, 0.01f, 1.0f);

			ImGui::ColorEdit4("FoamColor", &m_OceanMaterialParams.FoamColor[0]);

			ImGui::SliderFloat("FoamBiasLod0", &m_OceanMaterialParams.FoamBiasLod0, 0.01f, 7.0f);
			ImGui::SliderFloat("FoamBiasLod1", &m_OceanMaterialParams.FoamBiasLod1, 0.01f, 7.0f);
			ImGui::SliderFloat("FoamBiasLod2", &m_OceanMaterialParams.FoamBiasLod2, 0.01f, 7.0f);
			ImGui::SliderFloat("FoamScale", &m_OceanMaterialParams.FoamScale, 0.01f, 20.0f);
		}
		ImGui::End();
	}	
	ImGui::End();
}

void My_Water::WaterRender()
{
	OceanRenderParams OceanParams;
	//set swell
	m_WaveSwellSetting[0]->ConvertToCSSpectrumElementParam(&OceanParams.HKSpectrumParam.SpectrumElementParam[0]);
	m_WaveSwellSetting[1]->ConvertToCSSpectrumElementParam(&OceanParams.HKSpectrumParam.SpectrumElementParam[1]);
	//IFFT
	OceanParams.IFFTParam = IFFTBuffer({ mWaterTimer.GetWaterTime(), (float)WATER_FFT_N, (float)WATER_FFT_N/2, (float)m_Log2_N });
	//merge	
	float DeltaTime = mWaterTimer.GetWaterTime() - m_LastTimerCount;
	OceanParams.MergeParam = ResultMergeBuffer({1.0f, DeltaTime, float2(0.0f)});
	//std::cout << "DeltaTime = " << DeltaTime << std::endl;
	m_pOceanWave->ComputeOceanWave(m_pImmediateContext, OceanParams);
	m_LastTimerCount = mWaterTimer.GetWaterTime();
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

	mProfilersWindow.cpuGraph.LoadFrameData(pCPUData, CPUDataSize);
	mProfilersWindow.gpuGraph.LoadFrameData(pGPUData, GPUDataSize);	

	mProfilersWindow.Render();

	gRenderProfileMgr.CleanProfileTask();
}

My_Water::~My_Water()
{
	if (m_pOceanWave)
	{
		delete m_pOceanWave;
		m_pOceanWave = nullptr;
	}

	delete m_WaveSwellSetting[0];
	delete m_WaveSwellSetting[1];

	delete m_pReflectionProbe;
}

void My_Water::CreateGPUTexture()
{
	const int width = 128;

	TextureDesc TexDesc;
	TexDesc.Name = "Env cube map";
	TexDesc.Type = RESOURCE_DIM_TEX_CUBE;
	TexDesc.Usage = USAGE_DEFAULT;
	TexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
	TexDesc.Width = width;
	TexDesc.Height = width;
	TexDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
	TexDesc.ArraySize = 6;
	TexDesc.MipLevels = 0;

	m_pDevice->CreateTexture(TexDesc, nullptr, &m_apEnvCubemap);

	TextureDesc DepthBuffDesc;
	DepthBuffDesc.Type = RESOURCE_DIM_TEX_2D;
	DepthBuffDesc.Width = width;
	DepthBuffDesc.Height = width;
	DepthBuffDesc.MipLevels = 1;
	DepthBuffDesc.Name = "render cubemap depth buffer";
	DepthBuffDesc.Format = TEX_FORMAT_D32_FLOAT;
	DepthBuffDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
	m_pDevice->CreateTexture(DepthBuffDesc, nullptr, &m_apEnvCubemapDepth);
}

void My_Water::CubeMapRender()
{		
	if (m_PrecomputeEnvMapAttribsCB)
	{
		return;
	}
	/*if (!m_apEnvMapAttribsCB)
	{
		CreateUniformBuffer(m_pDevice, sizeof(PrecomputeEnvMapAttribs), "env map attribs CB", &m_apEnvMapAttribsCB);
	}*/

	FirstPersonCamera *pCameras = m_pReflectionProbe->GetCameras();

	for (int face = 0; face < 6; ++face)
	{
		TextureViewDesc RTVDesc(TEXTURE_VIEW_RENDER_TARGET, RESOURCE_DIM_TEX_2D_ARRAY);
		RTVDesc.Name = "RTV for cube texture";
		RTVDesc.FirstArraySlice = face;
		RTVDesc.NumArraySlices = 1;
		RefCntAutoPtr<ITextureView> pRTV;
		m_apEnvCubemap->CreateView(RTVDesc, &pRTV);
		ITextureView* ppRTVs[] = { pRTV };		

		//set rt for cubemap rendering
		//const float Zero[] = { 0.f, 0.f, 0.f, 0.f };
		auto* pDSV = m_apEnvCubemapDepth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
		m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		//m_pImmediateContext->ClearRenderTarget(pRTV, Zero, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		AtmosphereRender(&(pCameras[face]), pRTV, pDSV, m_apSkyScatteringCube.get(), false);
	}

	//Generate sky light map
	InitCubeMapFilterPSO();
	GetSkyDiffuse();
	GetSkySpec();
}

void My_Water::AtmosphereRender(FirstPersonCamera *pCam, ITextureView *pDstColor, ITextureView *pDstDepth, EpipolarLightScattering *pScattering, bool bNeedSun/* = true*/)
{
	CameraAttribs CamAttribs;
	CamAttribs.mViewT = pCam->GetViewMatrix().Transpose();
	CamAttribs.mProjT = pCam->GetProjMatrix().Transpose();
	CamAttribs.mViewProjT = pCam->GetViewProjMatrix().Transpose();
	CamAttribs.mViewProjInvT = pCam->GetViewProjMatrix().Inverse().Transpose();
	float fNearPlane = 0.f, fFarPlane = 0.f;
	pCam->GetProjMatrix().GetNearFarClipPlanes(fNearPlane, fFarPlane, false);
	CamAttribs.fNearPlaneZ = fNearPlane;
	CamAttribs.fFarPlaneZ = fFarPlane * 0.999999f;
	CamAttribs.f4Position = pCam->GetPos();
	CamAttribs.f4ViewportSize.x = pDstColor->GetTexture()->GetDesc().Width;// static_cast<float>(m_pSwapChain->GetDesc().Width);
	CamAttribs.f4ViewportSize.y = pDstColor->GetTexture()->GetDesc().Height;// static_cast<float>(m_pSwapChain->GetDesc().Height);
	CamAttribs.f4ViewportSize.z = 1.f / CamAttribs.f4ViewportSize.x;
	CamAttribs.f4ViewportSize.w = 1.f / CamAttribs.f4ViewportSize.y;

	EpipolarLightScattering::FrameAttribs FrameAttribs;

	FrameAttribs.pDevice = m_pDevice;
	FrameAttribs.pDeviceContext = m_pImmediateContext;
	//FrameAttribs.dElapsedTime = m_fElapsedTime;
	LightAttribs lattris;
	lattris.f4Direction = float4(m_LightManager.DirLight.dir, 0.0f);
	lattris.f4AmbientLight = float4(1, 1, 1, 1);
	float4 f4ExtraterrestrialSunColor = float4(10, 10, 10, 10) * m_LightManager.DirLight.intensity;
	lattris.f4Intensity = f4ExtraterrestrialSunColor; // *m_fScatteringScale;
	//lattris.f4AmbientLight = float4(0, 0, 0, 0);
	//lattris.f4Intensity = float4(m_LightManager.DirLight.intensity, m_LightManager.DirLight.intensity, m_LightManager.DirLight.intensity, m_LightManager.DirLight.intensity);
	FrameAttribs.pLightAttribs = &lattris;
	FrameAttribs.pCameraAttribs = &CamAttribs;

	/*FrameAttribs.pcbLightAttribs = m_pcbLightAttribs;
	FrameAttribs.pcbCameraAttribs = m_pcbCameraAttribs;	*/

	m_PPAttribs.uiNumSamplesOnTheRayAtDepthBreak = 32u;

	// During the ray marching, on each step we move by the texel size in either horz
	// or vert direction. So resolution of min/max mipmap should be the same as the
	// resolution of the original shadow map
	//m_PPAttribs.uiMinMaxShadowMapResolution = m_ShadowSettings.Resolution;
	m_PPAttribs.uiInitialSampleStepInSlice = std::min(m_PPAttribs.uiInitialSampleStepInSlice, m_PPAttribs.uiMaxSamplesInSlice);
	m_PPAttribs.uiEpipoleSamplingDensityFactor = std::min(m_PPAttribs.uiEpipoleSamplingDensityFactor, m_PPAttribs.uiInitialSampleStepInSlice);

	FrameAttribs.ptex2DSrcColorBufferSRV = m_pOffscreenColorBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
	if (bNeedSun)
	{
		FrameAttribs.ptex2DSrcDepthBufferSRV = m_pOffscreenDepthBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
	}
	else
	{
		FrameAttribs.ptex2DSrcDepthBufferSRV = pDstDepth->GetTexture()->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);//m_pOffscreenDepthBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
	}
	
	FrameAttribs.ptex2DDstColorBufferRTV = pDstColor; //m_pSwapChain->GetCurrentBackBufferRTV();
	FrameAttribs.ptex2DDstDepthBufferDSV = pDstDepth;// m_pSwapChain->GetDepthBufferDSV();
	//FrameAttribs.ptex2DShadowMapSRV = m_ShadowMapMgr.GetSRV();

	// Begin new frame
	pScattering->PrepareForNewFrame(FrameAttribs, m_PPAttribs);

	// Render the sun
	if(bNeedSun)
		pScattering->RenderSun(m_pOffscreenColorBuffer->GetDesc().Format, m_pOffscreenDepthBuffer->GetDesc().Format, 1);

	// Perform the post processing
	pScattering->PerformPostProcessing();
}

void My_Water::InitCubeMapFilterPSO()
{	
	TextureDesc TexDesc;
	TexDesc.Name = "Irradiance cube map for GLTF renderer";
	TexDesc.Type = RESOURCE_DIM_TEX_CUBE;
	TexDesc.Usage = USAGE_DEFAULT;
	TexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
	TexDesc.Width = IrradianceCubeDim;
	TexDesc.Height = IrradianceCubeDim;
	TexDesc.Format = IrradianceCubeFmt;
	TexDesc.ArraySize = 6;
	TexDesc.MipLevels = 1;

	RefCntAutoPtr<ITexture> IrradainceCubeTex;
	m_pDevice->CreateTexture(TexDesc, nullptr, &IrradainceCubeTex);
	m_pIrradianceCubeSRV = IrradainceCubeTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

	TexDesc.Name = "Prefiltered environment map for GLTF renderer";
	TexDesc.Width = PrefilteredEnvMapDim;
	TexDesc.Height = PrefilteredEnvMapDim;
	TexDesc.Format = PrefilteredEnvMapFmt;
	RefCntAutoPtr<ITexture> PrefilteredEnvMapTex;
	m_pDevice->CreateTexture(TexDesc, nullptr, &PrefilteredEnvMapTex);
	m_pPrefilteredEnvMapSRV = PrefilteredEnvMapTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

	if (!m_PrecomputeEnvMapAttribsCB)
	{
		CreateUniformBuffer(m_pDevice, sizeof(PrecomputeEnvMapAttribs), "Precompute env map attribs CB", &m_PrecomputeEnvMapAttribsCB);
	}

	ShaderCreateInfo ShaderCI;
	ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	ShaderCI.UseCombinedTextureSamplers = true;
	ShaderCI.pShaderSourceStreamFactory = m_pShaderSourceFactory;// &DiligentFXShaderSourceStreamFactory::GetInstance();

	{
		ShaderMacroHelper Macros;
		Macros.AddShaderMacro("NUM_PHI_SAMPLES", 64);
		Macros.AddShaderMacro("NUM_THETA_SAMPLES", 32);
		ShaderCI.Macros = Macros;
		RefCntAutoPtr<IShader> pVS;
		{
			ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
			ShaderCI.EntryPoint = "main";
			ShaderCI.Desc.Name = "Cubemap face VS";
			ShaderCI.FilePath = "assets/CubemapFace.vsh";
			m_pDevice->CreateShader(ShaderCI, &pVS);
		}

		// Create pixel shader
		RefCntAutoPtr<IShader> pPS;
		{
			ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
			ShaderCI.EntryPoint = "main";
			ShaderCI.Desc.Name = "Precompute irradiance cube map PS";
			ShaderCI.FilePath = "assets/ComputeIrradianceMap.psh";
			m_pDevice->CreateShader(ShaderCI, &pPS);
		}

		GraphicsPipelineStateCreateInfo PSOCreateInfo;
		PipelineStateDesc&              PSODesc = PSOCreateInfo.PSODesc;
		GraphicsPipelineDesc&           GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

		PSODesc.Name = "Precompute irradiance cube PSO";
		PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

		GraphicsPipeline.NumRenderTargets = 1;
		GraphicsPipeline.RTVFormats[0] = IrradianceCubeFmt;
		GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
		GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

		PSOCreateInfo.pVS = pVS;
		PSOCreateInfo.pPS = pPS;

		PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
		// clang-format off
		ShaderResourceVariableDesc Vars[] =
		{
			{SHADER_TYPE_PIXEL, "g_EnvironmentMap", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
		};
		// clang-format on
		PSODesc.ResourceLayout.NumVariables = _countof(Vars);
		PSODesc.ResourceLayout.Variables = Vars;

		// clang-format off
		ImmutableSamplerDesc ImtblSamplers[] =
		{
			{SHADER_TYPE_PIXEL, "g_EnvironmentMap", Sam_LinearClamp}
		};
		// clang-format on
		PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);
		PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;

		m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPrecomputeIrradianceCubePSO);
		m_pPrecomputeIrradianceCubePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbTransform")->Set(m_PrecomputeEnvMapAttribsCB);
		m_pPrecomputeIrradianceCubePSO->CreateShaderResourceBinding(&m_pPrecomputeIrradianceCubeSRB, true);
	}

	//Specular
	{
		ShaderMacroHelper Macros;
		Macros.AddShaderMacro("OPTIMIZE_SAMPLES", 1);
		ShaderCI.Macros = Macros;

		RefCntAutoPtr<IShader> pVS;
		{
			ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
			ShaderCI.EntryPoint = "main";
			ShaderCI.Desc.Name = "Cubemap face VS";
			ShaderCI.FilePath = "assets/CubemapFace.vsh";
			m_pDevice->CreateShader(ShaderCI, &pVS);
		}

		// Create pixel shader
		RefCntAutoPtr<IShader> pPS;
		{
			ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
			ShaderCI.EntryPoint = "main";
			ShaderCI.Desc.Name = "Prefilter environment map PS";
			ShaderCI.FilePath = "assets/PrefilterEnvMap.psh";
			m_pDevice->CreateShader(ShaderCI, &pPS);
		}

		GraphicsPipelineStateCreateInfo PSOCreateInfo;
		PipelineStateDesc&              PSODesc = PSOCreateInfo.PSODesc;
		GraphicsPipelineDesc&           GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

		PSODesc.Name = "Prefilter environment map PSO";
		PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

		GraphicsPipeline.NumRenderTargets = 1;
		GraphicsPipeline.RTVFormats[0] = PrefilteredEnvMapFmt;
		GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
		GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

		PSOCreateInfo.pVS = pVS;
		PSOCreateInfo.pPS = pPS;

		PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
		// clang-format off
		ShaderResourceVariableDesc Vars[] =
		{
			{SHADER_TYPE_PIXEL, "g_EnvironmentMap", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
		};
		// clang-format on
		PSODesc.ResourceLayout.NumVariables = _countof(Vars);
		PSODesc.ResourceLayout.Variables = Vars;

		// clang-format off
		ImmutableSamplerDesc ImtblSamplers[] =
		{
			{SHADER_TYPE_PIXEL, "g_EnvironmentMap", Sam_LinearClamp}
		};
		// clang-format on
		PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);
		PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;

		m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPrefilterEnvMapPSO);
		m_pPrefilterEnvMapPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbTransform")->Set(m_PrecomputeEnvMapAttribsCB);
		m_pPrefilterEnvMapPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "FilterAttribs")->Set(m_PrecomputeEnvMapAttribsCB);
		m_pPrefilterEnvMapPSO->CreateShaderResourceBinding(&m_pPrefilterEnvMapSRB, true);
	}	
}

void My_Water::GetSkyDiffuse()
{
	const std::array<float4x4, 6> Matrices =
	{
		/* +X */ float4x4::RotationY(+PI_F / 2.f),
		/* -X */ float4x4::RotationY(-PI_F / 2.f),
		/* +Y */ float4x4::RotationX(-PI_F / 2.f),
		/* -Y */ float4x4::RotationX(+PI_F / 2.f),
		/* +Z */ float4x4::Identity(),
		/* -Z */ float4x4::RotationY(PI_F)
	};

	m_pImmediateContext->SetPipelineState(m_pPrecomputeIrradianceCubePSO);
	m_pPrecomputeIrradianceCubeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_EnvironmentMap")->Set(m_apEnvCubemap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	m_pImmediateContext->CommitShaderResources(m_pPrecomputeIrradianceCubeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	auto*       pIrradianceCube = m_pIrradianceCubeSRV->GetTexture();
	const auto& IrradianceCubeDesc = pIrradianceCube->GetDesc();
	for (Uint32 mip = 0; mip < IrradianceCubeDesc.MipLevels; ++mip)
	{
		for (Uint32 face = 0; face < 6; ++face)
		{
			TextureViewDesc RTVDesc(TEXTURE_VIEW_RENDER_TARGET, RESOURCE_DIM_TEX_2D_ARRAY);
			RTVDesc.Name = "RTV for irradiance cube texture";
			RTVDesc.MostDetailedMip = mip;
			RTVDesc.FirstArraySlice = face;
			RTVDesc.NumArraySlices = 1;
			RefCntAutoPtr<ITextureView> pRTV;
			pIrradianceCube->CreateView(RTVDesc, &pRTV);
			ITextureView* ppRTVs[] = { pRTV };
			m_pImmediateContext->SetRenderTargets(_countof(ppRTVs), ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			{
				MapHelper<PrecomputeEnvMapAttribs> Attribs(m_pImmediateContext, m_PrecomputeEnvMapAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
				Attribs->Rotation = Matrices[face];
			}
			DrawAttribs drawAttrs(4, DRAW_FLAG_VERIFY_ALL);
			m_pImmediateContext->Draw(drawAttrs);
		}
	}
}

void My_Water::GetSkySpec()
{
	const std::array<float4x4, 6> Matrices =
	{
		/* +X */ float4x4::RotationY(+PI_F / 2.f),
		/* -X */ float4x4::RotationY(-PI_F / 2.f),
		/* +Y */ float4x4::RotationX(-PI_F / 2.f),
		/* -Y */ float4x4::RotationX(+PI_F / 2.f),
		/* +Z */ float4x4::Identity(),
		/* -Z */ float4x4::RotationY(PI_F)
	};

	m_pImmediateContext->SetPipelineState(m_pPrefilterEnvMapPSO);
	m_pPrefilterEnvMapSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_EnvironmentMap")->Set(m_apEnvCubemap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	m_pImmediateContext->CommitShaderResources(m_pPrefilterEnvMapSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	auto*       pPrefilteredEnvMap = m_pPrefilteredEnvMapSRV->GetTexture();
	const auto& PrefilteredEnvMapDesc = pPrefilteredEnvMap->GetDesc();
	for (Uint32 mip = 0; mip < PrefilteredEnvMapDesc.MipLevels; ++mip)
	{
		for (Uint32 face = 0; face < 6; ++face)
		{
			TextureViewDesc RTVDesc(TEXTURE_VIEW_RENDER_TARGET, RESOURCE_DIM_TEX_2D_ARRAY);
			RTVDesc.Name = "RTV for prefiltered env map cube texture";
			RTVDesc.MostDetailedMip = mip;
			RTVDesc.FirstArraySlice = face;
			RTVDesc.NumArraySlices = 1;
			RefCntAutoPtr<ITextureView> pRTV;
			pPrefilteredEnvMap->CreateView(RTVDesc, &pRTV);
			ITextureView* ppRTVs[] = { pRTV };
			m_pImmediateContext->SetRenderTargets(_countof(ppRTVs), ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			{
				MapHelper<PrecomputeEnvMapAttribs> Attribs(m_pImmediateContext, m_PrecomputeEnvMapAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
				Attribs->Rotation = Matrices[face];
				Attribs->Roughness = static_cast<float>(mip) / static_cast<float>(PrefilteredEnvMapDesc.MipLevels);
				Attribs->EnvMapDim = static_cast<float>(PrefilteredEnvMapDesc.Width);
				Attribs->NumSamples = 256;
			}

			DrawAttribs drawAttrs(4, DRAW_FLAG_VERIFY_ALL);
			m_pImmediateContext->Draw(drawAttrs);
		}
	}

	// clang-format off
	StateTransitionDesc Barriers[] =
	{
		{m_pPrefilteredEnvMapSRV->GetTexture(), RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, true},
		{m_pIrradianceCubeSRV->GetTexture(),    RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, true}
	};
	// clang-format on
	m_pImmediateContext->TransitionResourceStates(_countof(Barriers), Barriers);
}

WaterTimer::WaterTimer()
{
	mSpeed = 0.8f;
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
