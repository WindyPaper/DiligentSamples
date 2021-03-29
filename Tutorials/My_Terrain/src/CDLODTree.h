#ifndef _CDLOD_TREE_H_
#define _CDLOD_TREE_H_

#pragma once

#include <stdint.h>
#include "AdvancedMath.hpp"

#define MAX_LOD_COUNT 16

#define MAX_HEIGHTMAP_SIZE 65535
#define LEAF_RENDER_NODE_SIZE 8
#define LOD_COUNT  8

namespace Diligent
{
	struct HeightMap
	{
		uint16_t width;
		uint16_t height;
		uint16_t pitch;

		char* pData;

		//for test
		HeightMap()
		{
			width = 1024;
			height = 1024;
			pitch = 8;

			pData = NULL;
		}

		void GetZArea(const uint32_t& x, const uint32_t& y, const uint16_t& size, uint16_t& o_minz, uint16_t& o_maxz) const
		{
			o_minz = 0;
			o_maxz = 10;
		}

		uint16_t GetZ(const uint32_t& x, const uint32_t& y) const
		{
			return 10;
		}
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

		void Create(const int rx, const int ry, const int size, const int LodLevel, const HeightMap &heightmap, CDLODNode *pAllNodes, int &RefCurrUseNodeIdx);
	};

	class CDLODTree
	{
	public:
		explicit CDLODTree(const HeightMap &heightmap, const Dimension &TerrainDim);
		~CDLODTree();

		void Create();

	private:
		Dimension mTerrainDimension;
		HeightMap mHeightMap;

		CDLODNode*** mTopNodeArray;
		uint16_t mTopNodeNumX;
		uint16_t mTopNodeNumY;

		CDLODNode *mpNodeDataArray;
	};
}

#endif