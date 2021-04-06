#ifndef _CDLOD_TREE_H_
#define _CDLOD_TREE_H_

#pragma once

#include <stdint.h>
#include <vector>

#include "AdvancedMath.hpp"
#include "FirstPersonCamera.hpp"

#define MAX_LOD_COUNT 16

#define MAX_HEIGHTMAP_SIZE 65535
#define LEAF_RENDER_NODE_SIZE 8
#define LOD_COUNT  8
#define LOD_DISTANCE_RATIO 2.0f
#define LOD_MESH_GRID_SIZE 8

#include "TerrainMap.h"

namespace Diligent
{
	struct SelectionInfo;

	//struct TerrainMap
	//{
	//	uint16_t width;
	//	uint16_t height;
	//	uint16_t pitch;

	//	char* pData;

	//	//for test
	//	TerrainMap()
	//	{
	//		width = 1024;
	//		height = 1024;
	//		pitch = 8;

	//		pData = NULL;
	//	}

	//	void GetYArea(const uint32_t& x, const uint32_t& y, const uint16_t& size, uint16_t& o_minz, uint16_t& o_maxz) const
	//	{
	//		o_minz = 0;
	//		o_maxz = 50;
	//	}

	//	uint16_t GetY(const uint32_t& x, const uint32_t& y) const
	//	{
	//		return 50;
	//	}
	//};	

	enum class LODNodeState
	{
		UNDEFINED,
		OUT_OF_FRUSTUM,
		OUT_OF_LOD_RANGE,
		SELECTED
	};

	struct CDLODNode
	{
		//BoundBox aabb;
		int rx, ry, size;
		int LODLevel;
		uint16_t rminz, rmaxz;

		CDLODNode* pTL;
		CDLODNode* pTR;
		CDLODNode* pBL;
		CDLODNode* pBR;

		uint16_t flag;

		CDLODNode() :
			rx(0),
			ry(0),
			size(0),
			LODLevel(0),
			rminz(0),
			rmaxz(0),
			pTL(NULL),
			pTR(NULL),
			pBL(NULL),
			pBR(NULL),
			flag(0)
		{

		}

		void Create(const int rx, const int ry, const int size, const int LodLevel, TerrainMap &heightmap, CDLODNode *pAllNodes, int &RefCurrUseNodeIdx);
		LODNodeState SelectNode(SelectionInfo &SelectionNodes, bool bFullInFrustum);
		BoundBox GetBBox(const uint16_t RasSizeX, const uint16_t RasSizeY, const Dimension &TerrainDim);
	};

	struct SelectNodeData
	{
		CDLODNode *pNode;
		BoundBox aabb;
	};

	struct SelectionInfo
	{
		std::vector<SelectNodeData> SelectionNodes;
		uint16_t RasSizeX, RasSizeY;
		Dimension TerrainDimension;
		ViewFrustum frustum;
		float3 CamPos;
		std::vector<float> LODRange;

		float near, far;

		SelectionInfo() :
			RasSizeX(0),
			RasSizeY(0),
			near(-1.0f),
			far(-1.0f)
		{

		}
	};	

	class CDLODTree
	{
	public:
		explicit CDLODTree(const TerrainMap &heightmap, const Dimension &TerrainDim);
		~CDLODTree();

		void Create();
		void SelectLOD(const FirstPersonCamera &cam);

		const SelectionInfo &GetSelectInfo() const;

	private:
		//Dimension mTerrainDimension;
		TerrainMap mHeightMap;

		CDLODNode*** mTopNodeArray;
		uint16_t mTopNodeNumX;
		uint16_t mTopNodeNumY;

		CDLODNode *mpNodeDataArray;

		SelectionInfo mSelectionInfo;
	};
}

#endif