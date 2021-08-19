#include "OceanWave.h"

Diligent::WaveCascadeData::WaveCascadeData(IRenderDevice *pDevice, IPipelineState* pHKSpectrumPSO, IPipelineState* pIFFTPSO, ITexture* pGaussNoiseTex)
{
	m_pDevice = pDevice;

	m_pHKSpectrumPSO = pHKSpectrumPSO;
	m_pIFFTPSO = pIFFTPSO;

	m_pGaussNoiseTex = pGaussNoiseTex;	
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
	m_pDevice->CreateTexture(InitFloat2Type, nullptr, &m_HkSpectrum);
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
	m_pDevice->CreateTexture(InitFloat4Type, nullptr, &m_apBufferTmp0);
	m_pDevice->CreateTexture(InitFloat4Type, nullptr, &m_apBufferTmp1);
	m_pDevice->CreateTexture(InitFloat4Type, nullptr, &m_apDisplacement);

	InitFloat4Type.MipLevels = 0;  //full mipmap chain
	m_pDevice->CreateTexture(InitFloat4Type, nullptr, &m_apDerivatives);
	m_pDevice->CreateTexture(InitFloat4Type, nullptr, &m_apTurbulence);
}
