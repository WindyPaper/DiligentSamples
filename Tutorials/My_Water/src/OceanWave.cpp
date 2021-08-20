#include "OceanWave.h"

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
	m_pHKSpectrumPSO->CreateShaderResourceBinding(&m_apHKSpectrumSRB);
	m_pIFFTRowPSO->CreateShaderResourceBinding(&m_apIFFTRowSRB);
	m_pIFFTColumnPSO->CreateShaderResourceBinding(&m_apIFFTColumnSRB);
	m_pResultMergePSO->CreateShaderResourceBinding(&m_apResultMergeSRB);
}

void Diligent::WaveCascadeData::Init(const int N)
{
	InitTexture(N);
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

	BufferDesc ConstIFFTBuffer;
	ConstIFFTBuffer.Name = "IFFT Const Buffer Data";
	ConstIFFTBuffer.Usage = USAGE_DYNAMIC;
	ConstIFFTBuffer.BindFlags = BIND_UNIFORM_BUFFER;
	ConstIFFTBuffer.CPUAccessFlags = CPU_ACCESS_WRITE;
	ConstIFFTBuffer.uiSizeInBytes = sizeof(IFFTBuffer);
	m_pDevice->CreateBuffer(ConstIFFTBuffer, nullptr, &m_apIFFTParamsBuffer);

	BufferDesc ConstResultMergeBuffer;
	ConstResultMergeBuffer.Name = "Result Merge Buffer Data";
	ConstResultMergeBuffer.Usage = USAGE_DYNAMIC;
	ConstResultMergeBuffer.BindFlags = BIND_UNIFORM_BUFFER;
	ConstResultMergeBuffer.CPUAccessFlags = CPU_ACCESS_WRITE;
	ConstResultMergeBuffer.uiSizeInBytes = sizeof(IFFTBuffer);
	m_pDevice->CreateBuffer(ConstResultMergeBuffer, nullptr, &m_apResultMergeBuffer);
}

void Diligent::WaveCascadeData::ComputeWave(IDeviceContext *pContext)
{
	ComputeHKSpectrum(pContext);
	ComputeIFFT(pContext);
	ResultMerge(pContext);
}

void Diligent::WaveCascadeData::ComputeHKSpectrum(IDeviceContext *pContext)
{
	IShaderResourceVariable *pGauseNoise = m_apHKSpectrumSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::GAUSSNOISE);
	IShaderResourceVariable *pH0K = m_apHKSpectrumSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::HKSPECTRUM);
	IShaderResourceVariable *pWaveData = m_apHKSpectrumSRB->GetVariableByName(SHADER_TYPE_COMPUTE, WaveTextureSpace::WAVEDATA);

	pGauseNoise->Set(m_pGaussNoiseTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	pH0K->Set(m_apHkSpectrum->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
	pWaveData->Set(m_apWaveDataSpectrum->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));

	{
		MapHelper<HKSpectrumBuffer> CBConstants(pContext, m_apHKSpectrumParamsBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
		//CBConstants->SpectrumElementParam = 
	}
}

void Diligent::WaveCascadeData::ComputeIFFT(IDeviceContext *pContext)
{

}

void Diligent::WaveCascadeData::ResultMerge(IDeviceContext *pContext)
{

}

Diligent::OceanWave::OceanWave(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory) :
	m_pCascadeFar(nullptr),
	m_pCascadeMid(nullptr),
	m_pCascadeNear(nullptr)
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
	if (!m_pCascadeFar)
	{
		//m_pCascadeFar = new WaveCascadeData()
	}
	m_pCascadeFar->Init(N);
	m_pCascadeMid->Init(N);
	m_pCascadeNear->Init(N);
}

void Diligent::OceanWave::ComputeOceanWave(IDeviceContext *pContext)
{
	m_pCascadeFar->ComputeWave(pContext);
	m_pCascadeMid->ComputeWave(pContext);
	m_pCascadeNear->ComputeWave(pContext);
}

void Diligent::OceanWave::CreatePSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory)
{
	_CreateSpectrumPSO(pDevice, pShaderFactory);
	
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

	ShaderMacroHelper Macros;
	Macros.AddShaderMacro("THREAD_GROUP_SIZE", m_N);
	Macros.Finalize();

	RefCntAutoPtr<IShader> pCalculateSpectrumCS;
	{
		ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
		ShaderCI.EntryPoint = "CalculateInitialSpectrum";
		ShaderCI.Desc.Name = "Wave H0 Calculate";
		ShaderCI.FilePath = "wave_h0.csh";
		ShaderCI.Macros = Macros;
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
