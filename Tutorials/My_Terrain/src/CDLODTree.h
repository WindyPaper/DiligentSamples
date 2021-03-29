#ifndef _CDLOD_TREE_H_
#define _CDLOD_TREE_H_

#pragma once

#include <stdint.h>
#include "AdvancedMath.hpp"

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

		void GetZArea(const uint32_t& x, const uint32_t& y, const uint16_t& sizex, const uint16_t& sizey, int16_t& o_minz, int16_t& o_maxz)
		{
			o_minz = 0;
			o_maxz = 10;
		}

		uint16_t GetZ(const uint32_t& x, const uint32_t& y)
		{
			return 10;
		}
	};

	struct CDLODNode
	{
		BoundBox aabb;

		CDLODNode* pTL;
		CDLODNode* pTR;
		CDLODNode* pBL;
		CDLODNode* pBR;

		uint16_t flag;

		CDLODNode() :
			pTL(NULL),
			pTR(NULL),
			pBL(NULL),
			pBR(NULL),
			flag(0)
		{

		}
	};

	class CDLODTree
	{
	public:
		CDLODTree();
		~CDLODTree();

	private:
		Dimension mTerrainDimension;
		HeightMap mHeightMap;

		CDLODNode*** mTopNodeArray;
		uint16_t mTopNodeNumX;
		uint16_t mTopNodeNumY;


	};
}

#endif