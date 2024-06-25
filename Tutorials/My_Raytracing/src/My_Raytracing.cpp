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
#include "MapHelper.hpp"

#include "imgui.h"
#include "imGuIZMO.h"
#include "ImGuiUtils.hpp"

#include "assimp/postprocess.h"
#include "assimp/Exporter.hpp"

namespace Diligent
{

	struct SimpleObjConstants
	{
		float4x4 g_WorldViewProj;
		float4 g_CamPos;
		float4 g_BakeDirAndNum;

		float TestPlaneOffsetY;
		float BakeHeightScale;
		float BakeTexTiling;
		float padding;

		float4 bbox_min;
		float4 bbox_max;
	};

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
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    // Disable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
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
		ShaderCI.EntryPoint = "main";
		ShaderCI.Desc.Name = "simple obj VS";
		ShaderCI.FilePath = "simple_obj.vsh";
		m_pDevice->CreateShader(ShaderCI, &pVS);		
	}

	// Create a pixel shader
	RefCntAutoPtr<IShader> pPS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
		ShaderCI.EntryPoint = "main";
		ShaderCI.Desc.Name = "simple_obj PS";
		ShaderCI.FilePath = "simple_obj.psh";
		m_pDevice->CreateShader(ShaderCI, &pPS);
	}

	// Create dynamic uniform buffer that will store our transformation matrix
	// Dynamic buffers can be frequently updated by the CPU
	BufferDesc CBDesc;
	CBDesc.Name = "simple obj constants CB";
	CBDesc.uiSizeInBytes = sizeof(SimpleObjConstants);
	CBDesc.Usage = USAGE_DYNAMIC;
	CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
	CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
	m_pDevice->CreateBuffer(CBDesc, nullptr, &m_VSConstants);

	// Define variable type that will be used by default
	PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

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
		{ SHADER_TYPE_PIXEL, "g_Texture", Sam_LinearWrap }
	};
	// clang-format on
	PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;
	PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

	// Define vertex shader input layout
	LayoutElement LayoutElems[] =
	{
		// Attribute 0 - vertex position
		LayoutElement{0, 0, 3, VT_FLOAT32, False},
		// Attribute 1 - normal
		LayoutElement{1, 0, 3, VT_FLOAT32, False},
		//uv0
		LayoutElement{2, 0, 2, VT_FLOAT32, False},
		//uv1
		LayoutElement{3, 0, 2, VT_FLOAT32, False},
	};
	// clang-format on
	PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
	PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);		

    // Finally, create the pipeline state
    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);	

	// Since we did not explcitly specify the type for 'Constants' variable, default
	// type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
	// change and are bound directly through the pipeline state object.
	m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "SimpleObjConstants")->Set(m_VSConstants);
	m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "SimpleObjConstants")->Set(m_VSConstants);

	m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);

	//-----	

	CreateNormalObjPSO();

	//load raster mesh
	RasterMeshVec.emplace_back(load_mesh("./test_plane.fbx"));
	RasterMeshVec.emplace_back(load_mesh("./normal_obj.fbx"));

	float NearPlane = 0.1f;
	float FarPlane = 100000.f;
	float AspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
	m_Camera.SetProjAttribs(NearPlane, FarPlane, AspectRatio, PI_F / 4.f,
		m_pSwapChain->GetDesc().PreTransform, m_pDevice->GetDeviceCaps().IsGLDevice());
	m_Camera.SetSpeedUpScales(100.0f, 1000.0f);
	m_Camera.SetPos(float3(0.0f, 20.0f, -100.0f));
	m_Camera.SetMoveSpeed(10.0f);
	m_Camera.InvalidUpdate();

	mBakeHeightScale = 1.0f;
	mBakeTexTiling = 40.0f;
	mTestPlaneOffsetY = 2.0f;

	BakeInitDir = normalize(float3(-1.0f, -1.0f, 0.0f));

	std::vector<std::string> FileList;
	//FileList.emplace_back("high_poly_grass.FBX");
	//FileList.emplace_back("gzc_plant_grass_bai.FBX");
	FileList.emplace_back("test_grass.FBX");
	//FileList.emplace_back("test_combine_v.FBX");
	/*FileList.emplace_back("cjfj_guizi.fbx");
	FileList.emplace_back("heihufangjian_yugang.fbx");
	FileList.emplace_back("ws_guahua_men.fbx");
	FileList.emplace_back("xnjj_pingfeng02.fbx");
	FileList.emplace_back("xnjj_hua.fbx");	*/
		
	m_pMeshBVH = nullptr;
	m_pTrace = nullptr;
	//m_pPlaneMeshData = nullptr;

	for (int fidx = 0; fidx < FileList.size(); ++fidx)
	{
		if (m_pMeshBVH)
			delete m_pMeshBVH;
		if (m_pTrace)
			delete m_pTrace;

		m_pMeshBVH = new BVH(m_pImmediateContext, m_pDevice, m_pShaderSourceFactory, FileList[fidx]);
		m_pMeshBVH->BuildBVH();

		m_pTrace = new BVHTrace(m_pImmediateContext, m_pDevice, m_pShaderSourceFactory, m_pSwapChain, m_pMeshBVH, m_Camera, FileList[fidx]);
		//m_pTrace->DispatchVertexAOTrace();		
		//m_pTrace->DispatchTriangleAOTrace();

		//bake texture
		m_pTrace->DispatchBakeMesh3DTexture(BakeInitDir);

		m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_pTrace->GetBakeMesh3DTexture()->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	}

	/*IShaderResourceVariable *p_gtexture = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture");
	if (p_gtexture)
	{
		p_gtexture->Set(m_pTrace->GetOutputPixelTex()->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	}*/
	
}

// Render a frame
void MyRayTracing::Render()
{
	//m_pTrace->DispatchBakeMesh3DTexture();

	//m_pTrace->DispatchBVHTrace();

	//rasterization	
	auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
	auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
	// Clear the back buffer
	const float ClearColor[] = { 0.350f, 0.350f, 0.350f, 1.0f };
	m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	for each(RasterMeshData raster_data in RasterMeshVec)
	{
		//render plane
		{
			// Map the buffer and write current world-view-projection matrix
			MapHelper<SimpleObjConstants> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
			CBConstants->g_WorldViewProj = m_Camera.GetViewProjMatrix().Transpose();
			CBConstants->g_CamPos = m_Camera.GetPos();
			CBConstants->g_BakeDirAndNum = float4(BakeInitDir, BAKE_MESH_TEX_Z);

			CBConstants->BakeHeightScale = mBakeHeightScale;
			CBConstants->BakeTexTiling = mBakeTexTiling;
			CBConstants->TestPlaneOffsetY = mTestPlaneOffsetY;

			CBConstants->bbox_min = raster_data.bbox_min;
			CBConstants->bbox_max = raster_data.bbox_max;
		}

		RefCntAutoPtr<IPipelineState> apSelectPSO;
		RefCntAutoPtr<IShaderResourceBinding> apSelectSRB;

		if (raster_data.meshName.find("test_plane") != std::string::npos)
		{
			apSelectPSO = m_pPSO;
			apSelectSRB = m_pSRB;
		}
		else
		{
			apSelectPSO = m_pNormalObjPSO;
			apSelectSRB = m_pNormalObjSRB;
		}

		IShaderResourceVariable *pVSConstData = apSelectPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "SimpleObjConstants");
		if (pVSConstData)
		{
			pVSConstData->Set(m_VSConstants);
		}
		IShaderResourceVariable *pPSConstData = apSelectPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "SimpleObjConstants");
		if (pPSConstData)
		{
			pPSConstData->Set(m_VSConstants);
		}

		// Bind vertex and index buffers
		Uint32   offset = 0;
		IBuffer* pBuffs[] = { raster_data.apMeshVertexData };
		m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
		m_pImmediateContext->SetIndexBuffer(raster_data.apMeshIndexData, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		// Set the pipeline state
		m_pImmediateContext->SetPipelineState(apSelectPSO);
		// Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
		// makes sure that resources are transitioned to required states.
		m_pImmediateContext->CommitShaderResources(apSelectSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		DrawIndexedAttribs DrawAttrs;     // This is an indexed draw call
		DrawAttrs.IndexType = VT_UINT32; // Index type
		DrawAttrs.NumIndices = raster_data.pBVHMeshData->index_num;
		// Verify the state of vertex and index buffers
		DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
		m_pImmediateContext->DrawIndexed(DrawAttrs);
	}

	
	//m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

 //   DrawAttribs drawAttrs;
	//drawAttrs.NumVertices = 4;
	//drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL; // Verify the state of vertex and index buffers
 //   m_pImmediateContext->Draw(drawAttrs);
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

	for each (RasterMeshData data in RasterMeshVec)
	{
		delete data.pBVHMeshData;
	}
	RasterMeshVec.clear();
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

	ImGui::SliderFloat("TestPlaneOffsetY", &mTestPlaneOffsetY, 0.0f, 10.0f);
	ImGui::SliderFloat("BakeHeightScale", &mBakeHeightScale, 0.0f, 5.0f);
	ImGui::SliderFloat("BakeTexTiling", &mBakeTexTiling, 0.01f, 100.0f);

	ImGui::End();
}

RasterMeshData MyRayTracing::load_mesh(const std::string MeshName)
{
	/*if (m_pPlaneMeshData)
	{
		delete m_pPlaneMeshData;
		m_pPlaneMeshData = nullptr;
	}

	if (m_pPlaneMeshData == nullptr)
	{
		m_pPlaneMeshData = new BVHMeshData();
	}*/

	RasterMeshData mesh_data;	

	using namespace Assimp;
	Importer* p_assimp_importer = new Importer();
	unsigned int flags = aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_PreTransformVertices |
		aiProcess_RemoveRedundantMaterials |
		aiProcess_OptimizeMeshes |
		aiProcess_ConvertToLeftHanded;
	//"./test_plane.fbx"
	aiScene* p_import_fbx_scene = (aiScene*)(p_assimp_importer->ReadFile(MeshName, flags));

	if (p_import_fbx_scene == NULL)
	{
		std::string error_code = p_assimp_importer->GetErrorString();
		printf("Load FBX", "load fbx file failed! " + error_code);
		return mesh_data;
	}

	mesh_data.meshName = MeshName;
	mesh_data.pBVHMeshData = new BVHMeshData();

	int indices_offset = 0;
	unsigned int mesh_num = p_import_fbx_scene->mNumMeshes;

	std::vector<BVHVertex> mesh_vertex_data;
	std::vector<Uint32> mesh_index_data;
	
	for (unsigned int mesh_i = 0; mesh_i < mesh_num; ++mesh_i)
	{
		aiMesh* mesh_ptr = p_import_fbx_scene->mMeshes[mesh_i];
		const aiMaterial *mats = p_import_fbx_scene->mMaterials[mesh_ptr->mMaterialIndex];

		int vertex_num = mesh_ptr->mNumVertices;

		for (int i = 0; i < vertex_num; ++i)
		{
			const aiVector3D& v = mesh_ptr->mVertices[i];
			const aiVector3D& uv = mesh_ptr->mTextureCoords[0][i];
			aiVector3D uv1;
			if (mesh_ptr->mTextureCoords[1])
			{
				uv1 = mesh_ptr->mTextureCoords[1][i];
			}
			const aiVector3D& normal = mesh_ptr->mNormals[i];

			mesh_data.bbox_max.x = std::max(v.x, mesh_data.bbox_max.x);
			mesh_data.bbox_max.y = std::max(v.y, mesh_data.bbox_max.y);
			mesh_data.bbox_max.z = std::max(v.z, mesh_data.bbox_max.z);

			mesh_data.bbox_min.x = std::min(v.x, mesh_data.bbox_min.x);
			mesh_data.bbox_min.y = std::min(v.y, mesh_data.bbox_min.y);
			mesh_data.bbox_min.z = std::min(v.z, mesh_data.bbox_min.z);

			mesh_vertex_data.emplace_back(BVHVertex(float3(v.x, v.y, v.z), float3(normal.x, normal.y, normal.z), float2(uv.x, uv.y), float2(uv1.x, uv1.y)));
		}

		int triangle_num = mesh_ptr->mNumFaces;
		//int index_num = triangle_num * 3;
		for (int i = 0; i < triangle_num; ++i)
		{
			const aiFace& face = mesh_ptr->mFaces[i];
			//pMesh->triangles.emplace_back(Triangle{ face.mIndices[0], face.mIndices[1], face.mIndices[2] });
			for (int tri = 0; tri < 3; ++tri)
			{
				mesh_index_data.emplace_back(face.mIndices[tri] + indices_offset);
			}
		}
		indices_offset += vertex_num;
	}	

	// Create a vertex buffer that stores cube vertices
	BufferDesc VertBuffDesc;
	VertBuffDesc.Name = "mesh vertex buffer";
	VertBuffDesc.Usage = USAGE_IMMUTABLE;
	VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
	VertBuffDesc.uiSizeInBytes = sizeof(BVHVertex) * mesh_vertex_data.size();
	BufferData VBData;
	VBData.pData = &mesh_vertex_data[0];
	VBData.DataSize = VertBuffDesc.uiSizeInBytes;
	m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &mesh_data.apMeshVertexData);

	BufferDesc IndBuffDesc;
	IndBuffDesc.Name = "mesh index buffer";
	IndBuffDesc.Usage = USAGE_IMMUTABLE;
	IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;	
	IndBuffDesc.uiSizeInBytes = sizeof(Uint32) * mesh_index_data.size();
	BufferData IBData;
	IBData.pData = &mesh_index_data[0];
	IBData.DataSize = IndBuffDesc.uiSizeInBytes;
	m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &mesh_data.apMeshIndexData);

	mesh_data.pBVHMeshData->vertex_num = mesh_vertex_data.size();
	mesh_data.pBVHMeshData->index_num = mesh_index_data.size();

	if (p_assimp_importer)
	{
		p_assimp_importer->FreeScene();

		delete p_assimp_importer;
		p_assimp_importer = nullptr;
	}

	//RasterMeshVec.emplace_back(mesh_data);
	return mesh_data;
}

void MyRayTracing::CreateNormalObjPSO()
{
	GraphicsPipelineStateCreateInfo PSOCreateInfo;

	// Pipeline state name is used by the engine to report issues.
	// It is always a good idea to give objects descriptive names.
	PSOCreateInfo.PSODesc.Name = "Simple Normal Obj PSO";

	// This is a graphics pipeline
	PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

	// clang-format off
	// This tutorial will render to a single render target
	PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
	// Set render target format which is the format of the swap chain's color buffer
	PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
	// Use the depth buffer format from the swap chain
	PSOCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
	// Primitive topology defines what kind of primitives will be rendered by this pipeline state
	PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	// No back face culling for this tutorial
	PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
	// Disable depth testing
	PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
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
		ShaderCI.EntryPoint = "main";
		ShaderCI.Desc.Name = "normal obj VS";
		ShaderCI.FilePath = "normal_obj.vsh";
		m_pDevice->CreateShader(ShaderCI, &pVS);
	}

	// Create a pixel shader
	RefCntAutoPtr<IShader> pPS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
		ShaderCI.EntryPoint = "main";
		ShaderCI.Desc.Name = "normal obj PS";
		ShaderCI.FilePath = "normal_obj.psh";
		m_pDevice->CreateShader(ShaderCI, &pPS);
	}

	// Define variable type that will be used by default
	PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	//ShaderResourceVariableDesc Vars[] =
	//{
	//	{ SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE }
	//};
	//// clang-format on
	//PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
	//PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

	//// clang-format off
	//// Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
	//ImmutableSamplerDesc ImtblSamplers[] =
	//{
	//	{ SHADER_TYPE_PIXEL, "g_Texture", Sam_LinearWrap }
	//};
	//// clang-format on
	//PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;
	//PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

	// Define vertex shader input layout
	LayoutElement LayoutElems[] =
	{
		// Attribute 0 - vertex position
		LayoutElement{0, 0, 3, VT_FLOAT32, False},
		// Attribute 1 - normal
		LayoutElement{1, 0, 3, VT_FLOAT32, False},
		//uv0
		LayoutElement{2, 0, 2, VT_FLOAT32, False},
		//uv1
		LayoutElement{3, 0, 2, VT_FLOAT32, False},
	};
	// clang-format on
	PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
	PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

	// Finally, create the pipeline state
	PSOCreateInfo.pVS = pVS;
	PSOCreateInfo.pPS = pPS;
	m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pNormalObjPSO);

	// Since we did not explcitly specify the type for 'Constants' variable, default
	// type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
	// change and are bound directly through the pipeline state object.
	m_pNormalObjPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "SimpleObjConstants")->Set(m_VSConstants);
	if(m_pNormalObjPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "SimpleObjConstants"))
		m_pNormalObjPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "SimpleObjConstants")->Set(m_VSConstants);

	m_pNormalObjPSO->CreateShaderResourceBinding(&m_pNormalObjSRB, true);
}

} // namespace Diligent
