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

#include "My_Raytracing.hpp"
#include "BVH.h"
#include "BVHTrace.h"
#include "CommonlyUsedStates.h"

#include "imgui.h"
#include "imGuIZMO.h"
#include "ImGuiUtils.hpp"


namespace Diligent
{

SampleBase* CreateSample()
{
    return new MyRayTracing();
}

void MyRayTracing::Initialize(const SampleInitInfo& InitInfo)
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
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    // No back face culling for this tutorial
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    // Disable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.UseCombinedTextureSamplers = true;
	m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &m_pShaderSourceFactory);
	ShaderCI.pShaderSourceStreamFactory = m_pShaderSourceFactory;
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Triangle vertex shader";
        ShaderCI.FilePath		 = "simple_vs.vs";
        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Triangle pixel shader";
        ShaderCI.FilePath		 = "simple_ps.ps";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

	// Define variable type that will be used by default
	PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{ SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE }
	};
	// clang-format on
	PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
	PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

	// clang-format off
	// Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
	ImmutableSamplerDesc ImtblSamplers[] =
	{
		{ SHADER_TYPE_PIXEL, "g_Texture", Sam_LinearClamp }
	};
	// clang-format on
	PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;
	PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    // Finally, create the pipeline state
    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);
	m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);	

	//-----
	m_pMeshBVH = new BVH(m_pImmediateContext, m_pDevice, m_pShaderSourceFactory);
	m_pMeshBVH->BuildBVH();

	float NearPlane = 0.1f;
	float FarPlane = 100000.f;
	float AspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
	m_Camera.SetProjAttribs(NearPlane, FarPlane, AspectRatio, PI_F / 4.f,
		m_pSwapChain->GetDesc().PreTransform, m_pDevice->GetDeviceCaps().IsGLDevice());
	m_Camera.SetSpeedUpScales(100.0f, 1000.0f);
	m_Camera.SetPos(float3(0.0f, 0.0f, -50.0f));
	m_Camera.SetMoveSpeed(10.0f);
	m_Camera.InvalidUpdate();

	m_pTrace = new BVHTrace(m_pImmediateContext, m_pDevice, m_pShaderSourceFactory, m_pSwapChain, m_pMeshBVH, m_Camera);	

	IShaderResourceVariable *p_gtexture = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture");
	if (p_gtexture)
	{
		p_gtexture->Set(m_pTrace->GetOutputPixelTex()->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	}

	m_pTrace->GenVertexAORays();
}

// Render a frame
void MyRayTracing::Render()
{
	m_pTrace->DispatchBVHTrace();

    // Clear the back buffer
    const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
    // Let the engine perform required state transitions
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set the pipeline state in the immediate context
    m_pImmediateContext->SetPipelineState(m_pPSO);

	//m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_texture")->Set(m_pTrace->GetOutputPixelTex()->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	
	m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs drawAttrs;
	drawAttrs.NumVertices = 4;
	drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL; // Verify the state of vertex and index buffers
    m_pImmediateContext->Draw(drawAttrs);
}

void MyRayTracing::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

	m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));

	m_pTrace->Update(m_Camera);

	UpdateUI();
}

MyRayTracing::~MyRayTracing()
{
	if (m_pMeshBVH)
	{
		delete m_pMeshBVH;
		m_pMeshBVH = nullptr;
	}

	if (m_pTrace)
	{
		delete m_pTrace;
		m_pTrace = nullptr;
	}
}

void MyRayTracing::WindowResize(Uint32 Width, Uint32 Height)
{
	float NearPlane = 0.1f;
	float FarPlane = 100000.f;
	float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
	m_Camera.SetProjAttribs(NearPlane, FarPlane, AspectRatio, PI_F / 4.f,
		m_pSwapChain->GetDesc().PreTransform, m_pDevice->GetDeviceCaps().IsGLDevice());
	m_Camera.SetSpeedUpScales(100.0f, 300.0f);
}

void MyRayTracing::UpdateUI()
{
	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Camera Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		float3 CamPos = m_Camera.GetPos();
		ImGui::Text("Cam pos %.2f, %.2f, %.2f", CamPos.x, CamPos.y, CamPos.z);
		float3 CamForward = m_Camera.GetWorldAhead();
		ImGui::Text("Cam Forward %.2f, %.2f, %.2f", CamForward.x, CamForward.y, CamForward.z);
		ImGui::gizmo3D("Cam direction", CamForward, ImGui::GetTextLineHeight() * 10);
	}
	ImGui::End();
}

} // namespace Diligent
