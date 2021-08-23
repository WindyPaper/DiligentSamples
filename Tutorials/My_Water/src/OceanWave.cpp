#include "OceanWave.h"

#include "FastRand.hpp"

#include "MapHelper.hpp"
#include "ShaderMacroHelper.hpp"

Diligent::WaveCascadeData::WaveCascadeData(IRenderDevice *pDevice, \
	IPipelineState* pHKSpectrumPSO, IPipelineState* pIFFTRowPSO, IPipelineState* pIFFTColumnPSO, IPipelineState* pResultMergePSO, \
	ITexture* pGaussNoiseTex)
{
	m_pDevice = pDevice;

	m_pHKSpectrumPSO = pHKSpectrumPSO;
	m_pIFFTRowPSO = pIFFTRowPSO;
	m_pIFFTColumnPSO = pIFFTColumnPSO;
	m_pResultMergePSO = pResultMergePSO;

	m_pGaussNoiseTex = pGaussNoiseTex;

	//Create SRB
	m_pHKSpectrumPSO->CreateShaderResourceBinding(&m_apHKSpectrumSRB, true);
	m_pIFFTRowPSO->CreateShaderResourceBinding(&m_apIFFTRowSRB, true);
	m_pIFFTColumnPSO->CreateShaderResourceBinding(&m_apIFFTColumnSRB, true);
	m_pResultMergePSO->CreateShaderResourceBinding(&m_apResultMergeSRB, true);
}

void Diligent::WaveCascadeData::Init(const int N, const HKSpectrumGlobalParam &param)
{	
	m_HKScaleCutSpectrumData = param;
	InitTexture(N);
	InitBuffer();
}

void Diligent::WaveCascadeData::InitTexture(const int N)
{
	//float2 type
	TextureDesc InitFloat2Type;
	InitFloat2Type.Type = RESOURCE_DIM_TEX_2D;
	InitFloat2Type.Width = N;
	InitFloat2Type.Height = N;
	InitFloat2Type.MipLevels = 1;
	InitFloat2Type.Format = TEX_FORMAT_RG32_FLOAT;
	InitFloat2Type.Usage = USAGE_DYNAMIC;
	InitFloat2Type.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	m_pDevice->CreateTexture(InitFloat2Type, nullptr, &m_apHkSpectrum);
	m_pDevice->CreateTexture(InitFloat2Type, nullptr, &m_apDxDz);
	m_pDevice->CreateTexture(InitFloat2Type, nullptr, &m_apDyDxz);
	m_pDevice->CreateTexture(InitFloat2Type, nullptr, &m_apDyxDyz);
	m_pDevice->CreateTexture(InitFloat2Type, nullptr, &m_apDxxDzz);

	//float4 type
	TextureDesc InitFloat4Type;
	InitFloat4Type.Type = RESOURCE_DIM_TEX_2D;
	InitFloat4Type.Width = N;
	InitFloat4Type.Height = N;
	InitFloat4Type.MipLevels = 1;
	InitFloat4Type.Format = TEX_FORMAT_RGBA32_FLOAT;
	InitFloat4Type.Usage = USAGE_DYNAMIC;
	InitFloat4Type.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	m_pDevice->CreateTexture(InitFloat4Type, nullptr, &m_apWaveDataSpectrum);
	m_pDevice->CreateTexture(InitFloat4Type, nullptr, &m_apBufferTmp0);
	m_pDevice->CreateTexture(InitFloat4Type, nullptr, &m_apBufferTmp1);
	m_pDevice->CreateTexture(InitFloat4Type, nullptr, &m_apDisplacement);

	InitFloat4Type.MipLevels = 0;  //full mipmap chain
	m_pDevice->CreateTexture(InitFloat4Type, nullptr, &m_apDerivatives);
	m_pDevice->CreateTexture(InitFloat4Type, nullptr, &m_apTurbulence);
}

void Diligent::WaveCascadeData::InitBuffer()
{
	BufferDesc ConstHKSpectrumBuffer;
	ConstHKSpectrumBuffer.Name = "HKSpectrum Const Buffer Data";
	ConstHKSpectrumBuffer.Usage = USAGE_DYNAMIC;
	ConstHKSpectrumBuffer.BindFlags = BIND_UNIFORM_BUFFER;
	ConstHKSpectrumBuffer.CPUAccessFlags = CPU_ACCESS_WRITE;
	ConstHKSpectrumBuffer.uiSizeInBytes = sizeof(HKSpectrumBuffer);
	m_pDevice->CreateBuffer(ConstHKSpectrumBuffer, nullptr, &m_apHKSpectrumParamsBuffer);
	IShaderResourceVariable *pHKSpectrumBuffer = m_apHKSpectrumSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "cbHKSpectrumBuffer");
	pHKSpectrumBuffer->Set(m_apHKSpectrumParamsBuffer);

	BufferDesc ConstIFFTBuffer;
	ConstIFFTBuffer.Name = "IFFT Const Buffer Data";
	ConstIFFTBuffer.Usage = USAGE_DYNAMIC;
	ConstIFFTBuffer.BindFlags = BIND_UNIFORM_BUFFER;
	ConstIFFTBuffer.CPUAccessFlags = CPU_ACCESS_WRITE;
	ConstIFFTBuffer.uiSizeInBytes = sizeof(IFFTBuffer);
	m_pDevice->CreateBuffer(ConstIFFTBuffer, nullptr, &m_apIFFTParamsBuffer);
	IShaderResourceVariable *pIFFTBuffer = m_apIFFTColumnSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "cbIFFTBuffer");
	pIFFTBuffer->Set(m_apIFFTParamsBuffer);
	pIFFTBuffer = m_apIFFTRowSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "cbIFFTBuffer");
	pIFFTBuffer->Set(m_apIFFTParamsBuffer);

	BufferDesc ConstResultMergeBuffer;
	ConstResultMergeBuffer.Name = "Result Merge Buffer Data";
	ConstResultMergeBuffer.Usage = USAGE_DYNAMIC;
	ConstResultMergeBuffer.BindFlags = BIND_UNIFORM_BUFFER;
	ConstResultMergeBuffer.CPUAccessFlags = CPU_ACCESS_WRITE;
	ConstResultMergeBuffer.uiSizeInBytes = sizeof(IFFTBuffer);
	m_pDevice->CreateBuffer(ConstResultMergeBuffer, nullptr, &m_apResultMergeBuffer);
	IShaderResourceVariable *pResultMergeBuffer = m_apResultMergeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "cbResultMergeBuffer");
	pResultMergeBuffer->Set(m_apResultMergeBuffer);
}

void Diligent::WaveCascadeData::ComputeWave(IDeviceContext *pContext, const OceanRenderParams& params)
{
	ComputeHKSpectrum(pContext, params);
	ComputeIFFT(pContext, params);
	ResultMerge(pContext, params);
}

void Diligent::WaveCascadeData::ComputeHKSpectrum(IDeviceContext *pContext, const OceanRenderParams& params)
{
	pContext->SetPipelineState(m_apHKSpectrumSRB->GetPipelineState());

	IShaderResourceVariable *pGauseNoise = m_apHKSpectrumSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::GAUSSNOISE);
	IShaderResourceVariable *pH0K = m_apHKSpectrumSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::HKSPECTRUM);
	IShaderResourceVariable *pWaveData = m_apHKSpectrumSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::WAVEDATA);

	pGauseNoise->Set(m_pGaussNoiseTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	pH0K->Set(m_apHkSpectrum->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
	pWaveData->Set(m_apWaveDataSpectrum->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));	
	{
		MapHelper<HKSpectrumBuffer> CBConstants(pContext, m_apHKSpectrumParamsBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
		//CBConstants->SpectrumElementParam = 
		memcpy(CBConstants->SpectrumElementParam, &params.HKSpectrumParam.SpectrumElementParam, sizeof(HKSpectrumElementParam) * 2);
		CBConstants->SpectrumGlobalParam = m_HKScaleCutSpectrumData;
	}	

	pContext->CommitShaderResources(m_apHKSpectrumSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	

	const int LocalThreadsNum = 8;
	DispatchComputeAttribs DispAttr(m_HKScaleCutSpectrumData.N / LocalThreadsNum, m_HKScaleCutSpectrumData.N / LocalThreadsNum, 1);
	pContext->DispatchCompute(DispAttr);
}

void Diligent::WaveCascadeData::ComputeIFFT(IDeviceContext *pContext, const OceanRenderParams& params)
{
	//Column IFFT
	auto LocalComputeShaderSetParam = [&](IShaderResourceBinding* pSRB)
	{
		IShaderResourceVariable* pH0K = pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::HKSPECTRUM);
		IShaderResourceVariable* pBufferTmp0 = pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::BUFFERTMP0);
		IShaderResourceVariable* pBufferTmp1 = pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::BUFFERTMP1);
		IShaderResourceVariable* pDxDz = pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::DXDZ);
		IShaderResourceVariable* pDyDxz = pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::DYDXZ);
		IShaderResourceVariable* pDyxDyz = pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::DYXDYZ);
		IShaderResourceVariable* pDxxDzz = pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::DXXDZZ);
		IShaderResourceVariable* pWaveData = pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::WAVEDATA);

		pH0K->Set(m_apHkSpectrum->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
		pWaveData->Set(m_apWaveDataSpectrum->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

		pBufferTmp0->Set(m_apBufferTmp0->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
		pBufferTmp1->Set(m_apBufferTmp1->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
		if(pDxDz) pDxDz->Set(m_apDxDz->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
		if(pDyDxz) pDyDxz->Set(m_apDyDxz->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
		if(pDyxDyz) pDyxDyz->Set(m_apDyxDyz->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
		if(pDxxDzz) pDxxDzz->Set(m_apDxxDzz->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));

		//set const data
		{
			MapHelper<IFFTBuffer> CBConstants(pContext, m_apIFFTParamsBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
			*CBConstants = params.IFFTParam;
		}		
		pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	};

	pContext->SetPipelineState(m_apIFFTColumnSRB->GetPipelineState());
	LocalComputeShaderSetParam(m_apIFFTColumnSRB);	
	pContext->CommitShaderResources(m_apIFFTColumnSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	DispatchComputeAttribs IFFTColumnDisp(1, params.IFFTParam.N, 1);
	pContext->DispatchCompute(IFFTColumnDisp);

	//Row IFFT
	pContext->SetPipelineState(m_apIFFTRowSRB->GetPipelineState());
	LocalComputeShaderSetParam(m_apIFFTRowSRB);	
	pContext->CommitShaderResources(m_apIFFTRowSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	DispatchComputeAttribs IFFTRowDisp(1, params.IFFTParam.N, 1);
	pContext->DispatchCompute(IFFTRowDisp);
}

void Diligent::WaveCascadeData::ResultMerge(IDeviceContext *pContext, const OceanRenderParams& params)
{
	IShaderResourceVariable* pDxDz = m_apResultMergeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::DXDZ);
	IShaderResourceVariable* pDyDxz = m_apResultMergeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::DYDXZ);
	IShaderResourceVariable* pDyxDyz = m_apResultMergeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::DYXDYZ);
	IShaderResourceVariable* pDxxDzz = m_apResultMergeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::DXXDZZ);

	IShaderResourceVariable* pDisp = m_apResultMergeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::DISPLACEMENT);
	IShaderResourceVariable* pDerivatives = m_apResultMergeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::DERIVATIVES);
	IShaderResourceVariable* pTurbulence = m_apResultMergeSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::TURBULENCE);

	pDxDz->Set(m_apDxDz->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	pDyDxz->Set(m_apDyDxz->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	pDyxDyz->Set(m_apDyxDyz->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	pDxxDzz->Set(m_apDxxDzz->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

	pDisp->Set(m_apDisplacement->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
	pDerivatives->Set(m_apDerivatives->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
	pTurbulence->Set(m_apTurbulence->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));

	pContext->SetPipelineState(m_apResultMergeSRB->GetPipelineState());
	{
		MapHelper<ResultMergeBuffer> CBConstants(pContext, m_apResultMergeBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
		*CBConstants = params.MergeParam;
	}	

	pContext->CommitShaderResources(m_apResultMergeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	const int LocalThreadsNum = 8;
	DispatchComputeAttribs ResultMergeDisp(params.IFFTParam.N / LocalThreadsNum, params.IFFTParam.N / LocalThreadsNum, 1);
	pContext->DispatchCompute(ResultMergeDisp);
}

Diligent::OceanWave::OceanWave(const int N, IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory) :
	m_N(N),
	m_pDevice(pDevice),
	m_pCascadeFar(nullptr),
	m_pCascadeMid(nullptr),
	m_pCascadeNear(nullptr),
	m_LengthScale0(250.0f),
	m_LengthScale1(17.0f),
	m_LengthScale2(5.0f)
{
	CreatePSO(pDevice, pShaderFactory);
}

Diligent::OceanWave::~OceanWave()
{
	if (m_pCascadeFar)
	{
		delete m_pCascadeFar;
		m_pCascadeFar = nullptr;
	}

	if (m_pCascadeMid)
	{
		delete m_pCascadeMid;
		m_pCascadeMid = nullptr;
	}

	if (m_pCascadeNear)
	{
		delete m_pCascadeNear;
		m_pCascadeNear = nullptr;
	}
}

void Diligent::OceanWave::Init(const int N)
{
	m_N = N;
	if (!m_apGaussNoiseTex)
	{
		FastRandFloat r(100);
		auto NormalRandom = [&]()->float
		{
			return std::cosf(2 * PI_F * r() * std::sqrtf(-2 * std::logf(r())));
		};		
		float2 *pSrcTexData = new float2[N * N];
		for (int i = 0; i < N; ++i)
		{
			for (int j = 0; j < N; ++j)
			{
				pSrcTexData[i * N + j] = float2(NormalRandom(), NormalRandom());
			}
		}
		TextureData GaussTexData;
		TextureSubResData TexData;
		TexData.pData = pSrcTexData;
		TexData.Stride = sizeof(float2) * N;
		GaussTexData.pSubResources = &TexData;
		GaussTexData.NumSubresources = 1;

		TextureDesc TexDesc;
		TexDesc.Type = RESOURCE_DIM_TEX_2D;
		TexDesc.Width = N;
		TexDesc.Height = N;
		TexDesc.MipLevels = 1;
		TexDesc.Format = TEX_FORMAT_RG32_FLOAT;
		TexDesc.Usage = USAGE_DEFAULT;
		TexDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
		m_pDevice->CreateTexture(TexDesc, &GaussTexData, &m_apGaussNoiseTex);

		delete[] pSrcTexData;
	}

	if (!m_pCascadeFar)
	{
		m_pCascadeFar = new WaveCascadeData(m_pDevice, m_apHKSpectrumPSO, m_apIFFTRowPSO, m_apIFFTColumnPSO, m_apResultMergePSO, m_apGaussNoiseTex);
	}
	if (!m_pCascadeMid)
	{
		m_pCascadeMid = new WaveCascadeData(m_pDevice, m_apHKSpectrumPSO, m_apIFFTRowPSO, m_apIFFTColumnPSO, m_apResultMergePSO, m_apGaussNoiseTex);
	}
	if (!m_pCascadeNear)
	{
		m_pCascadeNear = new WaveCascadeData(m_pDevice, m_apHKSpectrumPSO, m_apIFFTRowPSO, m_apIFFTColumnPSO, m_apResultMergePSO, m_apGaussNoiseTex);
	}

	float boundary1 = 2 * PI_F / m_LengthScale1 * 6.0f;
	float boundary2 = 2 * PI_F/ m_LengthScale2 * 6.0f;	

	m_pCascadeFar->Init(N, HKSpectrumGlobalParam(m_N, m_LengthScale0, 0.0001f, boundary1));
	m_pCascadeMid->Init(N, HKSpectrumGlobalParam(m_N, m_LengthScale1, boundary1, boundary2));
	m_pCascadeNear->Init(N, HKSpectrumGlobalParam(m_N, m_LengthScale2, boundary2, 9999.9f));
}

void Diligent::OceanWave::ComputeOceanWave(IDeviceContext* pContext, const OceanRenderParams& params)
{
	m_pCascadeFar->ComputeWave(pContext, params);
	m_pCascadeMid->ComputeWave(pContext, params);
	m_pCascadeNear->ComputeWave(pContext, params);
}

void Diligent::OceanWave::CreatePSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory)
{
	_CreateSpectrumPSO(pDevice, pShaderFactory);
	_CreateIFFTColumnPSO(pDevice, pShaderFactory);
	_CreateIFFTRowPSO(pDevice, pShaderFactory);
	_CreateResultMergePSO(pDevice, pShaderFactory);
	
}

void Diligent::OceanWave::_CreateSpectrumPSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory)
{
	ShaderCreateInfo ShaderCI;
	ShaderCI.pShaderSourceStreamFactory = pShaderFactory;
	// Tell the system that the shader source code is in HLSL.
	// For OpenGL, the engine will convert this into GLSL under the hood.
	ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	// OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
	ShaderCI.UseCombinedTextureSamplers = true;

	//ShaderMacroHelper Macros;
	//Macros.AddShaderMacro("THREAD_GROUP_SIZE", 8);
	//Macros.Finalize();

	RefCntAutoPtr<IShader> pCalculateSpectrumCS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
		ShaderCI.EntryPoint = "CalculateInitialSpectrum";
		ShaderCI.Desc.Name = "Wave H0 Calculate";
		ShaderCI.FilePath = "wave_h0.csh";
		//ShaderCI.Macros = Macros;
		pDevice->CreateShader(ShaderCI, &pCalculateSpectrumCS);
	}

	ComputePipelineStateCreateInfo PSOCreateInfo;
	PipelineStateDesc&             PSODesc = PSOCreateInfo.PSODesc;

	// This is a compute pipeline
	PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

	PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
	//// clang-format off
	//ShaderResourceVariableDesc Vars[] =
	//{
	//	{SHADER_TYPE_COMPUTE, "HtOutput", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
	//	{SHADER_TYPE_COMPUTE, "DtOutput", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
	//	{SHADER_TYPE_COMPUTE, "H0", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
	//};
	//// clang-format on
	//PSODesc.ResourceLayout.Variables = Vars;
	//PSODesc.ResourceLayout.NumVariables = _countof(Vars);

	PSODesc.Name = "Wave H0 Compute shader";
	PSOCreateInfo.pCS = pCalculateSpectrumCS;
	pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apHKSpectrumPSO);
}

void Diligent::OceanWave::_CreateIFFTRowPSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory)
{
	ShaderCreateInfo ShaderCI;
	ShaderCI.pShaderSourceStreamFactory = pShaderFactory;
	// Tell the system that the shader source code is in HLSL.
	// For OpenGL, the engine will convert this into GLSL under the hood.
	ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	// OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
	ShaderCI.UseCombinedTextureSamplers = true;

	ShaderMacroHelper Macros;
	Macros.AddShaderMacro("THREAD_GROUP_SIZE", m_N);
	Macros.Finalize();

	RefCntAutoPtr<IShader> pIFFTRowCS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
		ShaderCI.EntryPoint = "RowInverseFFT";
		ShaderCI.Desc.Name = "Wave IFFT Row Calculate";
		ShaderCI.FilePath = "wave_ifft.csh";
		ShaderCI.Macros = Macros;
		pDevice->CreateShader(ShaderCI, &pIFFTRowCS);
	}

	ComputePipelineStateCreateInfo PSOCreateInfo;
	PipelineStateDesc&             PSODesc = PSOCreateInfo.PSODesc;

	// This is a compute pipeline
	PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

	PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;	

	PSODesc.Name = "Wave IFFT Row Compute shader";
	PSOCreateInfo.pCS = pIFFTRowCS;
	pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apIFFTRowPSO);
}

void Diligent::OceanWave::_CreateIFFTColumnPSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory)
{
	ShaderCreateInfo ShaderCI;
	ShaderCI.pShaderSourceStreamFactory = pShaderFactory;
	// Tell the system that the shader source code is in HLSL.
	// For OpenGL, the engine will convert this into GLSL under the hood.
	ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	// OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
	ShaderCI.UseCombinedTextureSamplers = true;

	ShaderMacroHelper Macros;
	Macros.AddShaderMacro("THREAD_GROUP_SIZE", m_N);
	Macros.Finalize();

	RefCntAutoPtr<IShader> pIFFTColumnCS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
		ShaderCI.EntryPoint = "RowInverseFFT";
		ShaderCI.Desc.Name = "Wave IFFT Column Calculate";
		ShaderCI.FilePath = "wave_ifft.csh";
		ShaderCI.Macros = Macros;
		pDevice->CreateShader(ShaderCI, &pIFFTColumnCS);
	}

	ComputePipelineStateCreateInfo PSOCreateInfo;
	PipelineStateDesc&             PSODesc = PSOCreateInfo.PSODesc;

	// This is a compute pipeline
	PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

	PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

	PSODesc.Name = "Wave IFFT Column Compute shader";
	PSOCreateInfo.pCS = pIFFTColumnCS;
	pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apIFFTColumnPSO);
}

void Diligent::OceanWave::_CreateResultMergePSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory)
{
	ShaderCreateInfo ShaderCI;
	ShaderCI.pShaderSourceStreamFactory = pShaderFactory;
	// Tell the system that the shader source code is in HLSL.
	// For OpenGL, the engine will convert this into GLSL under the hood.
	ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	// OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
	ShaderCI.UseCombinedTextureSamplers = true;

	ShaderMacroHelper Macros;
	Macros.AddShaderMacro("THREAD_GROUP_SIZE", m_N);
	Macros.Finalize();

	RefCntAutoPtr<IShader> pResultMergeCS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
		ShaderCI.EntryPoint = "FillResultTextures";
		ShaderCI.Desc.Name = "Wave Result merge";
		ShaderCI.FilePath = "wave_result_merge.csh";
		ShaderCI.Macros = Macros;
		pDevice->CreateShader(ShaderCI, &pResultMergeCS);
	}

	ComputePipelineStateCreateInfo PSOCreateInfo;
	PipelineStateDesc&             PSODesc = PSOCreateInfo.PSODesc;

	// This is a compute pipeline
	PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

	PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

	PSODesc.Name = "Wave result merge";
	PSOCreateInfo.pCS = pResultMergeCS;
	pDevice->CreateComputePipelineState(PSOCreateInfo, &m_apResultMergePSO);
}
