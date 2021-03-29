#include "CDLODTree.h"
#include <assert.h>
#include <algorithm>

namespace Diligent
{
	void CDLODNode::Create(const int rx, const int ry, const int size, const int LodLevel, const HeightMap &heightmap, CDLODNode *pAllNodes, int &RefCurrUseNodeIdx)
	{
		this->rx = rx;
		this->ry = ry;
		this->size = size;
		this->LODLevel = LodLevel;

		int heightmap_rx_size = heightmap.width;
		int heightmap_ry_size = heightmap.height;

		if (size == LEAF_RENDER_NODE_SIZE) //leaf
		{
			assert(LodLevel == LOD_COUNT - 1);

			heightmap.GetZArea(rx, ry, size, this->rminz, this->rmaxz);
		}
		else
		{
			auto SelectMinMaxFunc = [&](const CDLODNode *pNode)
			{
				this->rminz = std::min(this->rminz, pNode->rminz);
				this->rmaxz = std::max(this->rmaxz, pNode->rmaxz);
			};

			//Top Left
			int ChildSize = size / 2;
			this->pTL = &pAllNodes[RefCurrUseNodeIdx++];
			this->pTL->Create(rx, ry, ChildSize, LODLevel + 1, heightmap, pAllNodes, RefCurrUseNodeIdx);
			this->rminz = this->pTL->rminz;
			this->rmaxz = this->pTL->rmaxz;

			//Top Right
			if (rx + size < heightmap_rx_size)
			{
				this->pTR = &pAllNodes[RefCurrUseNodeIdx++];
				this->pTR->Create(rx + size, ry, ChildSize, LODLevel + 1, heightmap, pAllNodes, RefCurrUseNodeIdx);				
				SelectMinMaxFunc(this->pTR);
			}

			//Bottom Left
			if (ry + size < heightmap_ry_size)
			{
				this->pBL = &pAllNodes[RefCurrUseNodeIdx++];				
				this->pBL->Create(rx + size, ry, ChildSize, LODLevel + 1, heightmap, pAllNodes, RefCurrUseNodeIdx);
				SelectMinMaxFunc(this->pBL);
			}

			//Bottom Right
			if ((rx + size < heightmap_rx_size) && (ry + size < heightmap_ry_size))
			{
				this->pBR = &pAllNodes[RefCurrUseNodeIdx++];
				this->pBR->Create(rx + size, ry + size, ChildSize, LODLevel + 1, heightmap, pAllNodes, RefCurrUseNodeIdx);
				SelectMinMaxFunc(this->pBR);
			}
		}
	}


	CDLODTree::CDLODTree(const HeightMap &heightmap, const Dimension &TerrainDim) :
		mTopNodeArray(nullptr),
		mTopNodeNumX(0),
		mTopNodeNumY(0),
		mHeightMap(heightmap),
		mTerrainDimension(TerrainDim),
		mpNodeDataArray(nullptr)
	{

	}

	CDLODTree::~CDLODTree()
	{
		if (mTopNodeArray)
		{
			for (int j = 0; j < mTopNodeNumY; ++j)
			{
				for (int i = 0; i < mTopNodeNumX; ++i)
				{
					delete mTopNodeArray[i][j];
				}
			}

			mTopNodeArray = nullptr;
		}

		if (mpNodeDataArray)
		{
			delete[] mpNodeDataArray;
			mpNodeDataArray = nullptr;
		}		
	}


	void CDLODTree::Create()
	{
		uint16_t RasterW = mHeightMap.width;
		uint16_t RasterH = mHeightMap.height;

		//float TerrainX = mTerrainDimension.SizeX;
		//float TerrainZ = mTerrainDimension.SizeZ;

		int TopNodeSize = LEAF_RENDER_NODE_SIZE;
		int TotalNodeCount = 0;
		int TopCountX = 0;
		int TopCountY = 0;

		for (int i = 0; i < LOD_COUNT; ++i)
		{
			if (i != 0)
			{
				TopNodeSize *= 2;
			}

			int LevelLodNumX = (RasterW - 1) / TopNodeSize + 1;
			int LevelLodNumY = (RasterH - 1) / TopNodeSize + 1;

			TotalNodeCount += LevelLodNumX * LevelLodNumY;

			if (i == (LOD_COUNT - 1)) //TOP
			{
				TopCountX = LevelLodNumX;
				TopCountY = LevelLodNumY;
			}
		}

		//Construct tree
		mpNodeDataArray = new CDLODNode[TotalNodeCount];
		mTopNodeArray = new CDLODNode**[TopCountY];

		int CurrIdxNode = 0;
		for (int y = 0; y < TopCountY; ++y)
		{
			mTopNodeArray[y] = new CDLODNode*[TopCountX];
			for (int x = 0; x < TopCountX; ++x)
			{
				mTopNodeArray[y][x] = &mpNodeDataArray[CurrIdxNode++];
				mTopNodeArray[y][x]->Create(x * TopNodeSize, y * TopNodeSize, TopNodeSize, 0, mHeightMap, mpNodeDataArray, CurrIdxNode);
			}
		}

		assert(TotalNodeCount == CurrIdxNode);
	}
	

}