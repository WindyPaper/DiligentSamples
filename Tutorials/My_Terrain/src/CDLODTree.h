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

	enum class LODNodeState
	{
		UNDEFINED,
		OUT_OF_FRUSTUM,
		OUT_OF_LOD_RANGE,
		SELECTED
	};

	struct CDLODNode
	{
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

	//quad area
	struct SelectNodeAreaFlag
	{
		enum AreaFlag : uint8_t
		{
			TL_ON = 1,
			TR_ON = 1 << 1,
			BL_ON = 1 << 2,
			BR_ON = 1 << 3,
			
			FULL = 255
		};

		uint8_t flag;

		SelectNodeAreaFlag()
		{
			memset(this, 0, sizeof(SelectNodeAreaFlag));
		}

		SelectNodeAreaFlag(bool full)
		{
			if (full)
			{
				SetFull();
			}
			else
			{
				memset(this, 0, sizeof(SelectNodeAreaFlag));
			}			
		}

		SelectNodeAreaFlag(bool tl, bool tr, bool bl, bool br)
		{
			SetFlag(tl, tr, bl, br);
		}

		void SetFlag(bool tl, bool tr, bool bl, bool br)
		{
			if (tl == true && tr == true && bl == true && br == true)
			{
				SetFull();
			}
			else
			{
				flag |= (tl) | (tr << 1) | (bl << 2) | (br << 3);
			}						
		}

		void SetFull()
		{
			flag = FULL;
		}
	};

	struct SelectNodeData
	{
		CDLODNode *pNode;
		BoundBox aabb;
		SelectNodeAreaFlag AreaFlag;
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