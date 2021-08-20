#ifndef _OCEAN_WAVE_H_
#define _OCEAN_WAVE_H_

#include "BasicMath.hpp"
#include "DeviceContext.h"
#include "RefCntAutoPtr.hpp"
#include "RenderDevice.h"

namespace Diligent
{
	namespace WaveTextureSpace
	{
		static const char *WAVEDATA = "WavesData";
		static const char *HKSPECTRUM = "H0K";
		static const char *GAUSSNOISE = "Noise";

		static const char *DXDZ = "DxDz";
		static const char *DYDXZ = "DyDxz";
		static const char *DYXDYZ = "DyxDyz";
		static const char *DXXDZZ = "DxxDzz";
		static const char *BUFFERTMP0 = "BufferTmp0";
		static const char *BUFFERTMP1 = "BufferTmp1";

		static const char *DISPLACEMENT = "Displacement";
		static const char *DERIVATIVES = "Derivatives";
		static const char *TURBULENCE = "Turbulence";
	};

	struct HKSpectrumElementParam
	{
		float scale;
		float angle;
		float SpreadBlend;
		float swell;
		float alpha;
		float PeakOmega;
		float gamma;
		float ShortWavesFade;
	};

	struct HKSpectrumGlobalParam
	{
		float N;
		float LengthScale;
		float CutoffHigh;
		float CutoffLow;

		float GravityAcceleration;
		float Depth;
		float2 Padding;
	};

	struct HKSpectrumBuffer
	{
		HKSpectrumElementParam SpectrumElementParam[2];
		HKSpectrumGlobalParam SpectrumGlobalParam;
	};

	struct IFFTBuffer
	{
		float Time;
		float N;
		float Half_N;
		float BitNum;
	};

	struct ResultMergeBuffer
	{
		float Lambda;
		float DeltaTime;
		float2 Padding;
	};

	class WaveCascadeData
	{	
	public:
		WaveCascadeData(IRenderDevice *pDevice, \
			IPipelineState* pHKSpectrumPSO, IPipelineState* pIFFTRowPSO, IPipelineState* pIFFTColumnPSO, IPipelineState* pResultMergePSO, \
			ITexture* pGaussNoiseTex);

		void Init(const int N);

		void InitTexture(const int N);
		void InitBuffer();

		void ComputeWave(IDeviceContext *pContext);

	protected:
		void ComputeHKSpectrum(IDeviceContext *pContext);
		void ComputeIFFT(IDeviceContext *pContext);
		void ResultMerge(IDeviceContext *pContext);

	private:
		IRenderDevice *m_pDevice;
		IPipelineState *m_pHKSpectrumPSO;
		IPipelineState *m_pIFFTRowPSO;
		IPipelineState *m_pIFFTColumnPSO;
		IPipelineState *m_pResultMergePSO;
		ITexture *m_pGaussNoiseTex;

		RefCntAutoPtr<ITexture> m_apHkSpectrum; //float2	
		RefCntAutoPtr<ITexture> m_apWaveDataSpectrum; //float4

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

		//Buffer params
		RefCntAutoPtr<IBuffer> m_apHKSpectrumParamsBuffer;
		RefCntAutoPtr<IBuffer> m_apIFFTParamsBuffer;
		RefCntAutoPtr<IBuffer> m_apResultMergeBuffer;

		//SRB
		RefCntAutoPtr<IShaderResourceBinding> m_apHKSpectrumSRB;
		RefCntAutoPtr<IShaderResourceBinding> m_apIFFTRowSRB;
		RefCntAutoPtr<IShaderResourceBinding> m_apIFFTColumnSRB;
		RefCntAutoPtr<IShaderResourceBinding> m_apResultMergeSRB;

	};

	class OceanWave
	{
	public:
		OceanWave(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);

		~OceanWave();

		void Init(const int N);

		void ComputeOceanWave(IDeviceContext *pContext);

	protected:
		void CreatePSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);
		void _CreateSpectrumPSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);
		void _CreateIFFTRowPSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);
		void _CreateIFFTColumnPSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);
		void _CreateResultMergePSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);

	private:
		int m_N;

		WaveCascadeData *m_pCascadeNear;
		WaveCascadeData *m_pCascadeMid;
		WaveCascadeData *m_pCascadeFar;

		RefCntAutoPtr<IPipelineState> m_apHKSpectrumPSO;
		RefCntAutoPtr<IPipelineState> m_apIFFTRowPSO;
		RefCntAutoPtr<IPipelineState> m_apIFFTColumnPSO;
		RefCntAutoPtr<IPipelineState> m_apResultMergePSO;
	};
}

#endif
