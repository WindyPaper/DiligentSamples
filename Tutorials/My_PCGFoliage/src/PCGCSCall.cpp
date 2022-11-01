#include <fstream>      // std::ifstream

#include "PCGCSCall.h"
#include "MapHelper.hpp"

#include <assert.h>
#include "ShaderMacroHelper.hpp"
#include "PCGLayer.h"

Diligent::PCGCSCall::PCGCSCall(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory) :
	m_pDevice(pDevice)
{
	CreatePSO(pDevice, pShaderFactory);

	CreateGlobalPointTextureuBuffer();
}

Diligent::PCGCSCall::~PCGCSCall()
{

}

void Diligent::PCGCSCall::CreateGlobalPointBuffer(const std::vector<PCGPoint> &points)
{
	assert(mPointDeviceDataVec.size() == 0);

	mPointDeviceDataVec.resize(points.size());

	for (uint i = 0; i < points.size(); ++i)
	{
		/*BufferDesc PointBufferData;
		PointBufferData.Name = "PointData";
		PointBufferData.Usage = USAGE_DYNAMIC;
		PointBufferData.BindFlags = BIND_UNIFORM_BUFFER;
		PointBufferData.CPUAccessFlags = CPU_ACCESS_WRITE;
		PointBufferData.uiSizeInBytes = sizeof(float) * 2 * points[i].GetNum();*/

		uint TypeSize = sizeof(float) * 2;
		BufferDesc BuffDesc;
		BuffDesc.Name = "PCG Point Data buffer";
		BuffDesc.Usage = USAGE_IMMUTABLE;
		BuffDesc.BindFlags = BIND_SHADER_RESOURCE;
		BuffDesc.Mode = BUFFER_MODE_STRUCTURED;
		BuffDesc.ElementByteStride = TypeSize;
		BuffDesc.uiSizeInBytes = TypeSize * points[i].GetNum();

		BufferData VBData;
		VBData.pData = points[i].GetData();
		VBData.DataSize = TypeSize * points[i].GetNum();
		m_pDevice->CreateBuffer(BuffDesc, &VBData, &mPointDeviceDataVec[i]);
	}
}

void Diligent::PCGCSCall::CreateGlobalPointTextureuBuffer()
{
	mPointTextureDeviceDataVec.resize(F_LAYER_NUM);

	for (int i = 0; i < F_LAYER_NUM; ++i)
	{
		char DatName[128];
		sprintf(DatName, "./PoissonLayer%d.dat", i);

		std::ifstream is(DatName, std::ifstream::binary);

		int TextureSize, PointLen;

		is.read((char*)&TextureSize, sizeof(int));
		is.read((char*)&PointLen, sizeof(int));

		std::vector<float2> Points;
		Points.resize(PointLen);

		is.read((char*)&Points[0], sizeof(float2) * PointLen);
		is.close();

		const int TextureDataSize = TextureSize * TextureSize;
		std::vector<unsigned char> SrcTextureData;
		SrcTextureData.resize(TextureDataSize);

		memset(&SrcTextureData[0], 0, sizeof(unsigned char) * TextureDataSize);
		for (int pi = 0; pi < Points.size(); ++pi)
		{
			float2 &v = Points[pi];
			int x = v[0];
			int y = v[1];

			SrcTextureData[y * TextureSize + x] = 255;
		}

		//float type
		TextureDesc InitFloat4Type;
		InitFloat4Type.Type = RESOURCE_DIM_TEX_2D;
		InitFloat4Type.Width = TextureSize;
		InitFloat4Type.Height = TextureSize;
		InitFloat4Type.MipLevels = 1;
		InitFloat4Type.Format = TEX_FORMAT_R8_UNORM;
		InitFloat4Type.Usage = USAGE_DYNAMIC;
		InitFloat4Type.BindFlags = BIND_SHADER_RESOURCE;

		TextureData InitData;
		TextureSubResData TexData;
		TexData.pData = &SrcTextureData[0];
		TexData.Stride = sizeof(unsigned char) * TextureSize;
		InitData.pSubResources = &TexData;
		InitData.NumSubresources = 1;
		m_pDevice->CreateTexture(InitFloat4Type, &InitData, &mPointTextureDeviceDataVec[i]);
	}
}

void Diligent::PCGCSCall::CreateGPUGenSDFMapBuffer()
{
	//init
	uint TypeSize = sizeof(float4);
	BufferDesc BuffDesc;
	BuffDesc.Name = "PCG Init SDF Map Buffer";
	BuffDesc.Usage = USAGE_DYNAMIC;
	BuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
	BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
	BuffDesc.uiSizeInBytes = sizeof(PCGNodeData);
	
	m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_apInitSDFMapBuffer);

	//jump flood
	BuffDesc.Name = "PCG SDF Jump Flood buffer";
	m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_apSDFJumpFloodBuffer);

	//gen composite SDF
	BuffDesc.Name = "PCG Gen composite SDF buffer";
	m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_apGenSDFMapBuffer);
}

void Diligent::PCGCSCall::PosMapSetPSO(IDeviceContext *pContext)
{
	PCGCSGpuRes *pCalPosMapRes = &mPCGCSGPUResVec[PCG_CS_CAL_POS_MAP];
	pContext->SetPipelineState(pCalPosMapRes->apBindlessSRB->GetPipelineState());
}

void Diligent::PCGCSCall::BindPosMapPoints(uint Layer)
{
	PCGCSGpuRes *pCalPosMapRes = &mPCGCSGPUResVec[PCG_CS_CAL_POS_MAP];
	pCalPosMapRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "InPCGPointDatas")->Set(\
		mPointDeviceDataVec[Layer]->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
}

void Diligent::PCGCSCall::BindPoissonPosMap(uint Layer)
{
	PCGCSGpuRes *pCalPosMapRes = &mPCGCSGPUResVec[PCG_CS_CAL_POS_MAP];
	ITextureView *pPoissonTex = mPointTextureDeviceDataVec[Layer]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
	pCalPosMapRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "PoissonPosMapData")->Set(pPoissonTex);
}

void Diligent::PCGCSCall::BindPosMapRes(IDeviceContext *pContext, IBuffer *pNodeConstBuffer, const PCGNodeData &NodeData, ITexture* pOutTex)
{
	PCGCSGpuRes *pCalPosMapRes = &mPCGCSGPUResVec[PCG_CS_CAL_POS_MAP];

	/*pCalPosMapRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutPosMapData")->SetArray(\
		&TexDefaultArray[0], 0, TexDefaultArray.size());*/
	pCalPosMapRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutPosMapData")->Set(pOutTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));	

	{
		MapHelper<PCGNodeData> CBConstants(pContext, pNodeConstBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
		memcpy((void*)CBConstants.GetMapData(), &NodeData, sizeof(PCGNodeData));
	}
	pCalPosMapRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "cbPCGPointData")->Set(pNodeConstBuffer);
}

void Diligent::PCGCSCall::BindInitSDFMapData(IDeviceContext *pContext, const PCGNodeData &NodeData, ITexture *pInputTex, ITexture *pOutputTex)
{
	PCGCSGpuRes *pInitSDFMapRes = &mPCGCSGPUResVec[PCG_CS_INIT_SDF_MAP];

	{
		MapHelper<InitSDFMapData> CBConstants(pContext, m_apInitSDFMapBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
		InitSDFMapData inData;
		inData.TexSizeAndInvertSize = float4(NodeData.TexSize, NodeData.TexSize, 1.0f / NodeData.TexSize, 1.0f / NodeData.TexSize);
		memcpy((void*)CBConstants.GetMapData(), &inData, sizeof(InitSDFMapData));
	}
	pInitSDFMapRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "cbInitSDFMapData")->Set(m_apInitSDFMapBuffer);

	pInitSDFMapRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "InputTexture")->Set(pInputTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	pInitSDFMapRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutInitSDFMap")->Set(pOutputTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
}

void Diligent::PCGCSCall::BindSDFJumpFloodData(IDeviceContext *pContext, const PCGNodeData &NodeData, float2 SampleStep, ITexture *pInputTex, ITexture *pOutputTex)
{
	PCGCSGpuRes *pSDFJumpFloodRes = &mPCGCSGPUResVec[PCG_CS_SDF_JUMP_FLOOD_MAP];

	{
		MapHelper<PCGSDFJumpFloodData> CBConstants(pContext, m_apSDFJumpFloodBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
		PCGSDFJumpFloodData inData;
		inData.Step = SampleStep;
		inData.TextureSize = NodeData.TexSize;
		memcpy((void*)CBConstants.GetMapData(), &inData, sizeof(PCGSDFJumpFloodData));
	}
	pSDFJumpFloodRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "cbPCGSDFJumpFloodData")->Set(m_apSDFJumpFloodBuffer);

	pSDFJumpFloodRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "InputTexture")->Set(pInputTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	pSDFJumpFloodRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutputTexture")->Set(pOutputTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
}

void Diligent::PCGCSCall::BindGenSDFMapData(IDeviceContext *pContext, const PCGNodeData &NodeData, ITexture *pOriginalTex, ITexture *pInputTex, ITexture *pOutputTex)
{
	PCGCSGpuRes *pGenSDFMapRes = &mPCGCSGPUResVec[PCG_CS_GENERATE_SDF];

	{
		MapHelper<PCGGenSDFData> CBConstants(pContext, m_apGenSDFMapBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
		PCGGenSDFData inData;
		inData.TextureSize = float2(NodeData.TexSize, NodeData.TexSize);
		memcpy((void*)CBConstants.GetMapData(), &inData, sizeof(InitSDFMapData));
	}
	pGenSDFMapRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "cbPCGGenSDFData")->Set(m_apGenSDFMapBuffer);

	pGenSDFMapRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OriginalTexture")->Set(pOriginalTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	pGenSDFMapRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "InputTexture")->Set(pInputTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	pGenSDFMapRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutputTexture")->Set(pOutputTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
}

void Diligent::PCGCSCall::BindTerrainMaskMap(IDeviceContext *pContext, ITexture* pMaskTex)
{
	PCGCSGpuRes *pCalPosMapRes = &mPCGCSGPUResVec[PCG_CS_CAL_POS_MAP];	
	pCalPosMapRes->apBindlessSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "TerrainMaskMap")->Set(pMaskTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
}

void Diligent::PCGCSCall::PosMapDispatch(IDeviceContext *pContext, uint MapSize)
{
	PCGCSGpuRes *pCalPosMapRes = &mPCGCSGPUResVec[PCG_CS_CAL_POS_MAP];
	pContext->CommitShaderResources(pCalPosMapRes->apBindlessSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	DispatchComputeAttribs attr(MapSize / 4, MapSize / 4);
	pContext->DispatchCompute(attr);
}

void Diligent::PCGCSCall::CreatePSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory)
{
	assert(pDevice->GetDeviceCaps().Features.BindlessResources);

	CreateCalPosMapPSO(pDevice, pShaderFactory);

	CreateInitSDFMapPSO(pDevice, pShaderFactory);
	CreateSDFJumpFloodPSO(pDevice, pShaderFactory);
	CreateGenSDFMapPSO(pDevice, pShaderFactory);
}

void Diligent::PCGCSCall::CreateCalPosMapPSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory)
{
	PCGCSGpuRes *pCalPosMapRes = &mPCGCSGPUResVec[PCG_CS_CAL_POS_MAP];

	ShaderCreateInfo ShaderCI;
	ShaderCI.pShaderSourceStreamFactory = pShaderFactory;
	// Tell the system that the shader source code is in HLSL.
	// For OpenGL, the engine will convert this into GLSL under the hood.
	ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	// OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
	ShaderCI.UseCombinedTextureSamplers = true;

	/*ShaderMacroHelper Macros;
	Macros.AddShaderMacro("PCG_TILE_MAP_NUM", 8);
	Macros.Finalize();*/

	RefCntAutoPtr<IShader> pCalculatePOSMapCS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
		ShaderCI.EntryPoint = "CalculatePOSMap";
		ShaderCI.Desc.Name = "CalculatePOSMap CS";
		ShaderCI.FilePath = "CalculatePOSMap.csh";
		//ShaderCI.Macros = Macros;
		pDevice->CreateShader(ShaderCI, &pCalculatePOSMapCS);
	}

	ComputePipelineStateCreateInfo PSOCreateInfo;
	PipelineStateDesc&             PSODesc = PSOCreateInfo.PSODesc;

	// This is a compute pipeline
	PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

	PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "cbPCGPointData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},		
	};
	// clang-format on
	PSODesc.ResourceLayout.Variables = Vars;
	PSODesc.ResourceLayout.NumVariables = _countof(Vars);

	PSODesc.Name = "PCG cal map pos compute shader";
	PSOCreateInfo.pCS = pCalculatePOSMapCS;
	pDevice->CreateComputePipelineState(PSOCreateInfo, &pCalPosMapRes->apBindlessPSO);

	//SRB
	pCalPosMapRes->apBindlessPSO->CreateShaderResourceBinding(&pCalPosMapRes->apBindlessSRB, true);
}

void Diligent::PCGCSCall::CreateInitSDFMapPSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory)
{
	PCGCSGpuRes *pInitSDFMapRes = &mPCGCSGPUResVec[PCG_CS_INIT_SDF_MAP];

	ShaderCreateInfo ShaderCI;
	ShaderCI.pShaderSourceStreamFactory = pShaderFactory;
	// Tell the system that the shader source code is in HLSL.
	// For OpenGL, the engine will convert this into GLSL under the hood.
	ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	// OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
	ShaderCI.UseCombinedTextureSamplers = true;

	/*ShaderMacroHelper Macros;
	Macros.AddShaderMacro("PCG_TILE_MAP_NUM", 8);
	Macros.Finalize();*/

	RefCntAutoPtr<IShader> pInitSDFMapCS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
		ShaderCI.EntryPoint = "InitSDFMapMain";
		ShaderCI.Desc.Name = "InitSDFMapMain CS";
		ShaderCI.FilePath = "InitSDFMap.csh";
		//ShaderCI.Macros = Macros;
		pDevice->CreateShader(ShaderCI, &pInitSDFMapCS);
	}

	ComputePipelineStateCreateInfo PSOCreateInfo;
	PipelineStateDesc&             PSODesc = PSOCreateInfo.PSODesc;

	// This is a compute pipeline
	PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

	PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "cbInitSDFMapData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
	};
	// clang-format on
	PSODesc.ResourceLayout.Variables = Vars;
	PSODesc.ResourceLayout.NumVariables = _countof(Vars);

	PSODesc.Name = "PCG generate sdf map compute shader";
	PSOCreateInfo.pCS = pInitSDFMapCS;
	pDevice->CreateComputePipelineState(PSOCreateInfo, &pInitSDFMapRes->apBindlessPSO);

	//SRB
	pInitSDFMapRes->apBindlessPSO->CreateShaderResourceBinding(&pInitSDFMapRes->apBindlessSRB, true);
}

void Diligent::PCGCSCall::CreateSDFJumpFloodPSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory)
{
	PCGCSGpuRes *pSDFJumpFloodMapRes = &mPCGCSGPUResVec[PCG_CS_SDF_JUMP_FLOOD_MAP];

	ShaderCreateInfo ShaderCI;
	ShaderCI.pShaderSourceStreamFactory = pShaderFactory;
	// Tell the system that the shader source code is in HLSL.
	// For OpenGL, the engine will convert this into GLSL under the hood.
	ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	// OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
	ShaderCI.UseCombinedTextureSamplers = true;

	/*ShaderMacroHelper Macros;
	Macros.AddShaderMacro("PCG_TILE_MAP_NUM", 8);
	Macros.Finalize();*/

	RefCntAutoPtr<IShader> pSDFJumpFloodCS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
		ShaderCI.EntryPoint = "SDFJumpFloodMain";
		ShaderCI.Desc.Name = "SDFJumpFloodMain CS";
		ShaderCI.FilePath = "SDFJumpFlood.csh";
		//ShaderCI.Macros = Macros;
		pDevice->CreateShader(ShaderCI, &pSDFJumpFloodCS);
	}

	ComputePipelineStateCreateInfo PSOCreateInfo;
	PipelineStateDesc&             PSODesc = PSOCreateInfo.PSODesc;

	// This is a compute pipeline
	PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

	PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "cbPCGSDFJumpFloodData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
	};
	// clang-format on
	PSODesc.ResourceLayout.Variables = Vars;
	PSODesc.ResourceLayout.NumVariables = _countof(Vars);

	PSODesc.Name = "PCG generate sdf map compute shader";
	PSOCreateInfo.pCS = pSDFJumpFloodCS;
	pDevice->CreateComputePipelineState(PSOCreateInfo, &pSDFJumpFloodMapRes->apBindlessPSO);

	//SRB
	pSDFJumpFloodMapRes->apBindlessPSO->CreateShaderResourceBinding(&pSDFJumpFloodMapRes->apBindlessSRB, true);
}

void Diligent::PCGCSCall::CreateGenSDFMapPSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory)
{
	PCGCSGpuRes *pGenPosSDFMapRes = &mPCGCSGPUResVec[PCG_CS_GENERATE_SDF];

	ShaderCreateInfo ShaderCI;
	ShaderCI.pShaderSourceStreamFactory = pShaderFactory;
	// Tell the system that the shader source code is in HLSL.
	// For OpenGL, the engine will convert this into GLSL under the hood.
	ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	// OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
	ShaderCI.UseCombinedTextureSamplers = true;

	/*ShaderMacroHelper Macros;
	Macros.AddShaderMacro("PCG_TILE_MAP_NUM", 8);
	Macros.Finalize();*/

	RefCntAutoPtr<IShader> pGenSDFMapCS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
		ShaderCI.EntryPoint = "GenSDFMain";
		ShaderCI.Desc.Name = "GenSDFMain CS";
		ShaderCI.FilePath = "GenSDFMap.csh";
		//ShaderCI.Macros = Macros;
		pDevice->CreateShader(ShaderCI, &pGenSDFMapCS);
	}

	ComputePipelineStateCreateInfo PSOCreateInfo;
	PipelineStateDesc&             PSODesc = PSOCreateInfo.PSODesc;

	// This is a compute pipeline
	PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

	PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;

	// clang-format off
	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_COMPUTE, "cbPCGGenSDFData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
	};
	// clang-format on
	PSODesc.ResourceLayout.Variables = Vars;
	PSODesc.ResourceLayout.NumVariables = _countof(Vars);

	PSODesc.Name = "PCG generate sdf map compute shader";
	PSOCreateInfo.pCS = pGenSDFMapCS;
	pDevice->CreateComputePipelineState(PSOCreateInfo, &pGenPosSDFMapRes->apBindlessPSO);

	//SRB
	pGenPosSDFMapRes->apBindlessPSO->CreateShaderResourceBinding(&pGenPosSDFMapRes->apBindlessSRB, true);
}

