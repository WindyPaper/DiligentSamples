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

	class PCGTerrainTile
	{
	public:
		PCGTerrainTile(IDeviceContext *pContext, IRenderDevice *pDevice, const float2 &min, const float2 &size);

		void GenerateNodes(const PCGLayer *pLayer, const std::vector<PCGPoint> &PointVec);

		void GeneratePosMap(PCGCSCall *pPCGCall);

	protected:
		void CreateSpecificPCGTexture(const uint32_t Layer, const uint32_t LinearQuadIndex);
		void CreatePCGNodeDataBuffer(const std::vector<PCGNodeData> &PCGNodeDataVec);

		uint32_t GetLinearQuadIndex(const uint32_t Layer, const uint32_t MortonCode);
		void DivideTile(const PCGLayer *pLayer, const std::vector<PCGPoint> &PointVec);

	private:
		float2 mTileMin, mTileSize;

		//Generate texture hierarchy
		MortonCode mMortonCode;
		std::vector<RefCntAutoPtr<ITexture>> mGPUDensityTexArray; //quad tree
		std::vector<RefCntAutoPtr<ITexture>> mGPUSDFTexArray; //quad tree

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