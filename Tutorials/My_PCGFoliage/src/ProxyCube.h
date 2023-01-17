#ifndef _PROXY_CUBE_H_
#define _PROXY_CUBE_H_

#include "PCGLayer.h"
#include "BasicMath.hpp"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "RefCntAutoPtr.hpp"

namespace Diligent
{
	class FirstPersonCamera;

	class ProxyCube
	{
	public:
		ProxyCube();
		~ProxyCube();

		void InitPSO(IRenderDevice *pDevice, ISwapChain *pSwapChain);

		void CreateCubeBuffer(IRenderDevice* pDevice);
		void CreateInstBuffer(IRenderDevice* pDevice, IDeviceContext *pContext);		

		void SetFoliagePosData(uint32_t *pPlantLayerNum, std::vector<std::shared_ptr<float4[]>> &pPlantPositions);

		void Render(IDeviceContext *pContext, const FirstPersonCamera *pCam);

	protected:
		void PopulateInstanceBuffer(IDeviceContext *pContext, int LayerIdx, int InstCount);

	private:
		uint32_t* m_pPlantLayerNum;
		std::vector<std::shared_ptr<float4[]>> m_pPlantPositions;

		RefCntAutoPtr<IBuffer> m_apVertexBuffer;
		RefCntAutoPtr<IBuffer> m_apIndexBuffer;
		RefCntAutoPtr<IPipelineState> m_apPSO;
		RefCntAutoPtr<IBuffer> m_apInstanceBuffer[F_LAYER_NUM];
		RefCntAutoPtr<IShaderResourceBinding> m_SRB;
		RefCntAutoPtr<IBuffer> m_apVSConstants;
	};
}

#endif