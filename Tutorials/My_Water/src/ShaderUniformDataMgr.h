#ifndef _SHADER_UNIFORM_DATA_MGR_H_
#define _SHADER_UNIFORM_DATA_MGR_H_

#include "EngineFactory.h"
#include "RefCntAutoPtr.hpp"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "InputController.hpp"
#include "BasicMath.hpp"

namespace Diligent
{
#include "../assets/lighting_structure.fxh"

	struct OceanMaterialParams
	{
		float4 OceanColor;
		float4 SSSColor;

		float SSSStrength;
		float SSSScale;
		float SSSBase;
		float LodScale;

		float MaxGloss;
		float Roughness;
		float RoughnessScale;
		float ContactFoam;

		float4 FoamColor;

		float FoamBiasLod0;
		float FoamBiasLod1;
		float FoamBiasLod2;
		float FoamScale;		
	};

	class ShaderUniformDataMgr
	{
	public:
		ShaderUniformDataMgr();
		~ShaderUniformDataMgr();

		void CreateGPUBuffer(IRenderDevice *pDevice);

		IBuffer *GetLightStructure();

		const IBuffer *GetLightStructure() const;

	private:

		RefCntAutoPtr<IBuffer> m_apGPULightStructure;
	};
}

#endif