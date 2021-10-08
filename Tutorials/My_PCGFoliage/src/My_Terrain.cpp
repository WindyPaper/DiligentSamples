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

#include "My_Terrain.hpp"
#include "MapHelper.hpp"
#include "GroundMesh.h"
#include "DebugCanvas.h"
#include "imgui.h"
#include "imGuIZMO.h"
#include "ImGuiUtils.hpp"

namespace Diligent
{	
	DebugCanvas gDebugCanvas;

SampleBase* CreateSample()
{
    return new My_Terrain();
}

void My_Terrain::Initialize(const SampleInitInfo& InitInfo)
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
}

// Render a frame
void My_Terrain::Render()
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

	//render debug view
	//gDebugCanvas.Draw(m_pDevice, m_pSwapChain, m_pImmediateContext, m_pShaderSourceFactory, &m_Camera);
}

void My_Terrain::Update(double CurrTime, double ElapsedTime)
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

void My_Terrain::CreateGridBuffer()
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

void My_Terrain::WindowResize(Uint32 Width, Uint32 Height)
{
	float NearPlane = 0.1f;
	float FarPlane = 100000.f;
	float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
	m_Camera.SetProjAttribs(NearPlane, FarPlane, AspectRatio, PI_F / 4.f,
		m_pSwapChain->GetDesc().PreTransform, m_pDevice->GetDeviceCaps().IsGLDevice());
	m_Camera.SetSpeedUpScales(100.0f, 300.0f);
}

void My_Terrain::UpdateUI()
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
