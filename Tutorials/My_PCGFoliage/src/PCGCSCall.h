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
		PCG_CS_INIT_SDF_MAP,
		PCG_CS_SDF_JUMP_FLOOD_MAP,
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

	struct InitSDFMapData
	{
		float4 TexSizeAndInvertSize;
	};

	struct PCGSDFJumpFloodData
	{
		float2 Step;
		float2 TextureSize;
	};

	struct PCGGenSDFData
	{
		float2 TextureSize;
		float PlantRadius;
		float PlantZOI; // zone of influence
	};

	class PCGCSCall
	{
	public:
		PCGCSCall(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);
		~PCGCSCall();

		void CreateGlobalPointBuffer(const std::vector<PCGPoint> &points);
		void CreateGlobalPointTextureuBuffer();

		void CreateGPUGenSDFMapBuffer();

		void PosMapSetPSO(IDeviceContext *pContext);
		void BindPosMapPoints(uint Layer);
		void BindPoissonPosMap(uint Layer);
		void BindPosMapRes(IDeviceContext *pContext, IBuffer *pNodeConstBuffer, const PCGNodeData &NodeData, ITexture* pOutTex);

		void InitSDFMapSetPSO(IDeviceContext *pContext);
		void BindInitSDFMapData(IDeviceContext *pContext, const PCGNodeData &NodeData, ITexture *pInputTex, ITexture *pOutputTex);
		void InitSDFDispatch(IDeviceContext *pContext, uint MapSize);

		void SDFJumpFloodSetPSO(IDeviceContext *pContext);
		void BindSDFJumpFloodData(IDeviceContext *pContext, const PCGNodeData &NodeData, float2 SampleStep, ITexture *pInputTex, ITexture *pOutputTex, bool reverse);
		void SDFJumpFloodDispatch(IDeviceContext *pContext, uint MapSize);

		void GenSDFMapSetPSO(IDeviceContext *pContext);
		void BindGenSDFMapData(IDeviceContext *pContext, const PCGNodeData &NodeData, ITexture *pOriginalTex, ITexture *pInputTex, ITexture *pOutputTex, bool reverse);
		void GenSDFMapDispatch(IDeviceContext *pContext, uint MapSize);

		void BindTerrainMaskMap(IDeviceContext *pContext, ITexture* pMaskTex);

		void PosMapDispatch(IDeviceContext *pContext, uint MapSize);

		//void UpdatePosMapData(const PCGNodeData *pPCGNode);

	protected:
		void CreatePSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);

		void CreateCalPosMapPSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);
		void CreateInitSDFMapPSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);
		void CreateSDFJumpFloodPSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);
		void CreateGenSDFMapPSO(IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);

	private:
		PCGCSGpuRes mPCGCSGPUResVec[PCG_CS_NUM];

		std::vector<RefCntAutoPtr<IBuffer>> mPointDeviceDataVec;
		std::vector<RefCntAutoPtr<ITexture>> mPointTextureDeviceDataVec;
		//RefCntAutoPtr<IBuffer> mPosMapNodeData;

		//SDF Buffer
		RefCntAutoPtr<IBuffer> m_apInitSDFMapBuffer;
		RefCntAutoPtr<IBuffer> m_apSDFJumpFloodBuffer;
		RefCntAutoPtr<IBuffer> m_apGenSDFMapBuffer;

		IRenderDevice *m_pDevice;
	};
}

#endif