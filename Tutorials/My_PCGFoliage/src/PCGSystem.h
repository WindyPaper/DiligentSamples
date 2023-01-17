#ifndef _PCG_SYSTEM_H_
#define _PCG_SYSTEM_H_

#include <vector>

#include "RefCntAutoPtr.hpp"
#include "BasicMath.hpp"
#include "Texture.h"

#include "PCGLayer.h"
#include "PCGNodePool.h"
#include "PCGPoint.h"
#include "PCGCSCall.h"
#include "PoissonDisk/PoissonDiskSampling.h"
#include "MortonCode.h"

#include "AdvancedMath.hpp"

namespace Diligent
{
	struct IRenderDevice;
	struct IDeviceContext;	

	inline uint32_t GetPCGTextureNum()
	{
		uint32_t texNum = 0;
		for (int i = 0; i < F_LAYER_NUM; ++i)
		{
			texNum += static_cast<uint32_t>(std::pow(4, i + 1));
		}

		return texNum;
	}

	struct PCGResultData
	{
		std::vector<std::shared_ptr<float4[]>> PlantPositionHostDatas;
		std::shared_ptr<uint32_t[]> PlantTypeNumHostData;

		PCGResultData(std::vector<std::shared_ptr<float4[]>> &PosDatas, std::shared_ptr<uint32_t[]> TypeNum) :
			PlantPositionHostDatas(PosDatas),
			PlantTypeNumHostData(TypeNum)
		{}
	};

	class PCGTerrainTile
	{
	public:
		PCGTerrainTile(IDeviceContext *pContext, IRenderDevice *pDevice, const float2 &min, const float2 &size);

		void InitGlobalRes();

		void GenerateNodes(const PCGLayer *pLayer, const std::vector<PCGPoint> &PointVec);

		void GeneratePosMap(PCGCSCall *pPCGCall);
		//void GenerateSDFMap(PCGCSCall *pPCGCall);

		void GenerateSDFMap(PCGCSCall *pPCGCall, int index);

		void ReadBackPositionDataToHost();

		PCGResultData GetPCGResultData();

	protected:
		void CreateSpecificPCGTexture(const uint32_t Layer, const uint32_t LinearQuadIndex);
		void CreatePCGNodeDataBuffer(const std::vector<PCGNodeData> &PCGNodeDataVec);

		uint32_t GetLinearQuadIndex(const uint32_t Layer, const uint32_t MortonCode);
		uint32_t GetParentIndex(const uint32_t LinearQuadIndex);

		void DivideTile(const PCGLayer *pLayer, const std::vector<PCGPoint> &PointVec);

	private:
		float2 mTileMin, mTileSize;

		//Generate texture hierarchy
		MortonCode mMortonCode;
		std::vector<RefCntAutoPtr<ITexture>> mGPUDensityTexArray; //quad tree
		std::vector<RefCntAutoPtr<ITexture>> mGPUSDFTexArrayPing; //quad tree
		std::vector<RefCntAutoPtr<ITexture>> mGPUSDFTexArrayPong; //quad tree
		std::vector<RefCntAutoPtr<ITexture>> mGPUSDFResultTexArray; //quad tree

		//position buffer  type-positions data of GPU
		RefCntAutoPtr<IBuffer> mPlantPositionBuffers;

		//layer plant type number data of GPU
		RefCntAutoPtr<IBuffer> mPlantTypeNumBuffer;

		RefCntAutoPtr<ITexture> mGlobalTerrainMaskTex;

		//CPU data
		std::vector<std::shared_ptr<float4[]>> mPlantPositionHostDatas;
		std::shared_ptr<uint32_t[]> mPlantTypeNumHostData;

		//To CPU stage data
		RefCntAutoPtr<IBuffer> mPlantPosStageDatas;
		RefCntAutoPtr<IBuffer> mPlantTypeNumStageData;

		RefCntAutoPtr<IFence>  mPlantStageDataAvailable;

		RefCntAutoPtr<IBuffer> mPCGGPUNodeConstBuffer;
		std::vector<PCGNodeData> mPCGNodeDataVec;

		IDeviceContext *m_pContext;
		IRenderDevice *m_pRenderDevice;
	};

	class PCGSystem
	{
	public:
		PCGSystem(IDeviceContext *pContext, IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory, const Dimension &TerrainDim);
		~PCGSystem();

		void Init();

		void CreatePoissonDiskSamplerData();

		void DoProcedural();

		PCGResultData GetPCGResultData();

		//void GeneratePCGTextureArray();

	//protected:		

	private:
		IDeviceContext *m_pContext;
		IRenderDevice *m_pRenderDevice;

		PCGLayer mPlantLayer;

		uint32_t mSeed;
		std::vector<PCGPoint> mPointVec;		

		PCGCSCall mPCGCSCall;

		PCGNodePool mNodePool;
		
		PCGTerrainTile *mTerrainTile;

		Dimension mTerrainDim;		
	};
}

#endif