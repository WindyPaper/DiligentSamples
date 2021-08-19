#ifndef _OCEAN_WAVE_H_
#define _OCEAN_WAVE_H_

#include "BasicMath.hpp"
#include "DeviceContext.h"
#include "RefCntAutoPtr.hpp"
#include "RenderDevice.h"

namespace Diligent
{
	struct HKSpectrumElementParam
	{
		float scale;
		float angle;
		float spreadBlend;
		float swell;
		float alpha;
		float peakOmega;
		float gamma;
		float shortWavesFade;
	};

	struct HKSpectrumGlobalParam
	{
		float Size;
		float LengthScale;
		float CutoffHigh;
		float CutoffLow;

		float GravityAcceleration;
		float Depth;
		float2 Padding;
	};

	class WaveCascadeData
	{	
	public:
		WaveCascadeData(IRenderDevice *pDevice, IPipelineState* pHKSpectrumPSO, IPipelineState* pIFFTPSO, ITexture* pGaussNoiseTex);

		void Init(const int N);

		void InitTexture(const int N);

		void ComputeWave(IDeviceContext *pContext);

	protected:
		void ComputeHKSpectrum(IDeviceContext *pContext);

	private:
		IRenderDevice *m_pDevice;
		IPipelineState *m_pHKSpectrumPSO;
		IPipelineState *m_pIFFTPSO;
		ITexture *m_pGaussNoiseTex;

		RefCntAutoPtr<ITexture> m_HkSpectrum; //float2		

		RefCntAutoPtr<ITexture> m_apDxDz; //float2
		RefCntAutoPtr<ITexture> m_apDyDxz; //float2
		RefCntAutoPtr<ITexture> m_apDyxDyz; //float2
		RefCntAutoPtr<ITexture> m_apDxxDzz; //float2

		RefCntAutoPtr<ITexture> m_apBufferTmp0; //float4
		RefCntAutoPtr<ITexture> m_apBufferTmp1; //float4

		RefCntAutoPtr<ITexture> m_apDisplacement; //float4
		//need to generate mipmap
		RefCntAutoPtr<ITexture> m_apDerivatives; //float4
		RefCntAutoPtr<ITexture> m_apTurbulence; //float4
	};

	class OceanWave
	{
	public:
		OceanWave()
		{

		}


	};
}

#endif
