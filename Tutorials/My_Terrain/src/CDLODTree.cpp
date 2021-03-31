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
			if (rx + ChildSize < heightmap_rx_size)
			{
				this->pTR = &pAllNodes[RefCurrUseNodeIdx++];
				this->pTR->Create(rx + ChildSize, ry, ChildSize, LODLevel + 1, heightmap, pAllNodes, RefCurrUseNodeIdx);
				SelectMinMaxFunc(this->pTR);
			}

			//Bottom Left
			if (ry + ChildSize < heightmap_ry_size)
			{
				this->pBL = &pAllNodes[RefCurrUseNodeIdx++];				
				this->pBL->Create(rx, ry + ChildSize, ChildSize, LODLevel + 1, heightmap, pAllNodes, RefCurrUseNodeIdx);
				SelectMinMaxFunc(this->pBL);
			}

			//Bottom Right
			if ((rx + ChildSize < heightmap_rx_size) && (ry + ChildSize < heightmap_ry_size))
			{
				this->pBR = &pAllNodes[RefCurrUseNodeIdx++];
				this->pBR->Create(rx + ChildSize, ry + ChildSize, ChildSize, LODLevel + 1, heightmap, pAllNodes, RefCurrUseNodeIdx);
				SelectMinMaxFunc(this->pBR);
			}
		}
	}


	LODNodeState CDLODNode::SelectNode(SelectionInfo &SelectInfo, bool bFullInFrustum)
	{
		//if not choose any childs, select self
		//bool bHasSelectChild = false;

		BoundBox bbox = this->GetBBox(SelectInfo.RasSizeX, SelectInfo.RasSizeY, SelectInfo.TerrainDimension);

		BoxVisibility BoxVis = BoxVisibility::FullyVisible;		
		if (!bFullInFrustum)
		{			
			BoxVis = GetBoxVisibility(SelectInfo.frustum, bbox);

			if (BoxVis == BoxVisibility::Invisible)
			{
				return LODNodeState::OUT_OF_FRUSTUM;
			}
		}

		//LOD distance
		float LODDistance = SelectInfo.LODRange[this->LODLevel];
		if (!bbox.IntersectSphereSq(SelectInfo.CamPos, LODDistance * LODDistance))
		{
			return LODNodeState::OUT_OF_LOD_RANGE;
		}

		//Child Visible
		LODNodeState TLState = LODNodeState::UNDEFINED;
		LODNodeState TRState = LODNodeState::UNDEFINED;
		LODNodeState BLState = LODNodeState::UNDEFINED;
		LODNodeState BRState = LODNodeState::UNDEFINED;
		if (this->LODLevel < LOD_COUNT)
		{
			float NextLODDistance = SelectInfo.LODRange[this->LODLevel + 1];
			if (!bbox.IntersectSphereSq(SelectInfo.CamPos, NextLODDistance * NextLODDistance))
			{
				bool NodeFullVisible = BoxVis == BoxVisibility::FullyVisible;
				TLState = this->pTL->SelectNode(SelectInfo, NodeFullVisible);
				TRState = this->pTR->SelectNode(SelectInfo, NodeFullVisible);
				BLState = this->pBL->SelectNode(SelectInfo, NodeFullVisible);
				BRState = this->pBR->SelectNode(SelectInfo, NodeFullVisible);
			}			
		}
		else		
		{
			//Leaf
			SelectInfo.SelectionNodes.push_back(this);
			return LODNodeState::SELECTED;
		}

		auto SelectChildNodeFunc = [&](LODNodeState state, CDLODNode* pNode)
		{
			if (state == LODNodeState::OUT_OF_LOD_RANGE)
			{
				SelectInfo.SelectionNodes.push_back(pNode);
			}
		};
		SelectChildNodeFunc(TLState, this->pTL);
		SelectChildNodeFunc(TRState, this->pTR);
		SelectChildNodeFunc(BLState, this->pBL);
		SelectChildNodeFunc(BRState, this->pBR);


		return LODNodeState::OUT_OF_LOD_RANGE;
	}

	Diligent::BoundBox CDLODNode::GetBBox(const uint16_t RasSizeX, const uint16_t RasSizeY, const Dimension &TerrainDim)
	{		
		BoundBox bbox;
		float Minx = (float)this->rx / (RasSizeX - 1) * TerrainDim.SizeX;
		float Miny = (float)this->ry / (RasSizeY - 1) * TerrainDim.SizeX;
		float Minz = (float)this->rminz / MAX_HEIGHTMAP_SIZE * TerrainDim.SizeZ;

		float Maxx = (float)(this->rx + size) / (RasSizeX - 1) * TerrainDim.SizeX;
		float Maxy = (float)(this->ry + size) / (RasSizeY - 1) * TerrainDim.SizeX;
		float Maxz = (float)this->rmaxz / MAX_HEIGHTMAP_SIZE * TerrainDim.SizeZ;

		bbox.Min = float3({ Minx, Miny, Minz }) + TerrainDim.Min;
		bbox.Max = float3({ Maxx, Maxy, Maxz }) + TerrainDim.Min;

		return bbox;

	}

	CDLODTree::CDLODTree(const HeightMap &heightmap, const Dimension &TerrainDim) :
		mTopNodeArray(nullptr),
		mTopNodeNumX(0),
		mTopNodeNumY(0),
		mHeightMap(heightmap),
		mpNodeDataArray(nullptr)
	{
		mSelectionInfo.RasSizeX = heightmap.width;
		mSelectionInfo.RasSizeY = heightmap.height;
		mSelectionInfo.TerrainDimension = TerrainDim;
	}

	CDLODTree::~CDLODTree()
	{
		if (mTopNodeArray)
		{			
			for (int i = 0; i < mTopNodeNumX; ++i)
			{
				delete[] mTopNodeArray[i];
			}
			
			delete[] mTopNodeArray;
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
				mTopNodeNumX = LevelLodNumX;
				mTopNodeNumY = LevelLodNumY;
			}
		}

		//Construct tree
		mpNodeDataArray = new CDLODNode[TotalNodeCount];
		mTopNodeArray = new CDLODNode**[mTopNodeNumY];

		int CurrIdxNode = 0;
		for (int y = 0; y < mTopNodeNumY; ++y)
		{
			mTopNodeArray[y] = new CDLODNode*[mTopNodeNumX];
			for (int x = 0; x < mTopNodeNumX; ++x)
			{
				mTopNodeArray[y][x] = &mpNodeDataArray[CurrIdxNode++];
				mTopNodeArray[y][x]->Create(x * TopNodeSize, y * TopNodeSize, TopNodeSize, 0, mHeightMap, mpNodeDataArray, CurrIdxNode);
			}
		}

		assert(TotalNodeCount == CurrIdxNode);
		LOG_INFO_MESSAGE("CDLOD Tree Memory: ", sizeof(CDLODNode) * TotalNodeCount / 1024.0f, " KB");
	}
	

	void CDLODTree::SelectLOD(const FirstPersonCamera &cam)
	{
		mSelectionInfo.SelectionNodes.clear();
		mSelectionInfo.CamPos = cam.GetPos();

		//Update LOD distance range
		if (mSelectionInfo.near != cam.GetProjAttribs().NearClipPlane &&
			mSelectionInfo.far != cam.GetProjAttribs().FarClipPlane)
		{
			mSelectionInfo.near = cam.GetProjAttribs().NearClipPlane;
			mSelectionInfo.far = cam.GetProjAttribs().FarClipPlane;

			mSelectionInfo.LODRange.resize(LOD_COUNT);
			std::vector<float> &LODRange = mSelectionInfo.LODRange;

			//update LOD range
			float total = 0.0f;
			float distance = 1.0f;
			for (int i = 0; i < LOD_COUNT; ++i)
			{				
				total += distance;
				distance *= LOD_DISTANCE_RATIO;
			}

			float CamViewDistance = mSelectionInfo.far - mSelectionInfo.near;
			float SectUnit = CamViewDistance / total;

			float PrevPos = mSelectionInfo.near;
			distance = 1.0f;
			for (int i = 0; i < LOD_COUNT; ++i)
			{
				int index = LOD_COUNT - i - 1;
				LODRange[index] = PrevPos + SectUnit * distance;
				PrevPos = LODRange[index];
				distance *= LOD_DISTANCE_RATIO;
			}
		}

		ExtractViewFrustumPlanesFromMatrix(cam.GetViewProjMatrix(), mSelectionInfo.frustum, false);

		for (int y = 0; y < mTopNodeNumY; ++y)
		{
			for (int x = 0; x < mTopNodeNumX; ++x)
			{
				CDLODNode *pNode = mTopNodeArray[y][x];
				BoundBox bbox = pNode->GetBBox(mHeightMap.width, mHeightMap.height, mSelectionInfo.TerrainDimension);

				BoxVisibility vis = GetBoxVisibility(mSelectionInfo.frustum, bbox);

				//bool bBoxVis = false;
				if (vis == BoxVisibility::FullyVisible)
				{
					LODNodeState state = pNode->SelectNode(mSelectionInfo, true);
					if (state == LODNodeState::OUT_OF_LOD_RANGE)
					{
						mSelectionInfo.SelectionNodes.push_back(pNode);
					}
				}
				else if (vis == BoxVisibility::Intersecting)
				{
					LODNodeState state = pNode->SelectNode(mSelectionInfo, false);
					if (state == LODNodeState::OUT_OF_LOD_RANGE)
					{
						mSelectionInfo.SelectionNodes.push_back(pNode);
					}
				}
				
			}
		}
	}

	const Diligent::SelectionInfo & CDLODTree::GetSelectInfo() const
	{
		return mSelectionInfo;
	}

}