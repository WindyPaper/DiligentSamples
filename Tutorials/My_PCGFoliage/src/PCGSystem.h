#ifndef _PCG_SYSTEM_H_
#define _PCG_SYSTEM_H_

#include "PCGLayer.h"
#include "PCGNodePool.h"
#include "PCGPoint.h"
#include "PoissonDisk/PoissonDiskSampling.h"

namespace Diligent
{
	struct IRenderDevice;
	struct IDeviceContext;

	class PCGSystem
	{
	public:
		PCGSystem(IDeviceContext *pContext, IRenderDevice *pDevice);
		~PCGSystem();

		void CreatePoissonDiskSamplerData();

		void DoProcedural(SelectionInfo *pNodeInfo);

	private:
		IDeviceContext *m_pContext;
		IRenderDevice *m_pRenderDevice;

		PCGLayer mPlantLayer;

		uint32_t mSeed;
		std::vector<PCGPoint> mPointVec;

		PCGNodePool mNodePool;
	};
}

#endif