#ifndef _PCG_CS_CALL_H_
#define _PCG_CS_CALL_H_

#include <unordered_map>

#include "RefCntAutoPtr.hpp"
#include "Buffer.h"
#include "Texture.h"
#include "PipelineState.h"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "Shader.h"
#include "PCGPoint.h"
#include "PCGNodePool.h"

namespace Diligent
{
	struct PCGNodeData;

	enum PCG_CS_STAGE
	{
		PCG_CS_CAL_POS_MAP,
		PCG_CS_GENERATE_SDF,
		PCG_CS_EVALUATE_POS,
		PCG_CS_NUM
	};	

	struct PCGCSGpuRes
	{
		RefCntAutoPtr<IPipelineState>         apBindlessPSO;		
		RefCntAutoPtr<IShaderResourceBinding> apBindlessSRB;
		//std::unordered_map<std::string, RefCntAutoPtr<IBuffer>>   apCSBuffers;
	};

	class PCGCSCall
	{
	public:
		PCGCSCall(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);
		~PCGCSCall();

		void CreateGlobalPointBuffer(const std::vector<PCGPoint> &points);

		void PosMapSetPSO(IDeviceContext *pContext);
		void BindPosMapPoints(uint Layer);
		void BindPosMapRes(IDeviceContext *pContext, IBuffer *pNodeConstBuffer, const PCGNodeData &NodeData, ITexture* pOutTex);
		void PosMapDispatch(IDeviceContext *pContext, uint MapSize);

		//void UpdatePosMapData(const PCGNodeData *pPCGNode);

	protected:
		void CreatePSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);

		void CreateCalPosMapPSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);

	private:
		PCGCSGpuRes mPCGCSGPUResVec[PCG_CS_NUM];

		std::vector<RefCntAutoPtr<IBuffer>> mPointDeviceDataVec;
		//RefCntAutoPtr<IBuffer> mPosMapNodeData;

		IRenderDevice *m_pDevice;
	};
}

#endif