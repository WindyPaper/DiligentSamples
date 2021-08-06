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