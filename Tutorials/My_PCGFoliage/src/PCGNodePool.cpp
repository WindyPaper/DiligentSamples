#include "PCGNodePool.h"
#include "CDLODTree.h"

Diligent::PCGNodePool::PCGNodePool()
{

}

Diligent::PCGNodePool::~PCGNodePool()
{

}

void Diligent::PCGNodePool::QueryNodes(SelectionInfo *pNodeInfo)
{
	auto &RefLODNodes = pNodeInfo->SelectionNodes;

	for (int i = 0; i < RefLODNodes.size(); ++i)
	{
		CDLODNode* pNodeData = RefLODNodes[i].pNode;
		if (pNodeData->LODLevel == LOD_COUNT - 1) //Leaf
		{
			if (mPrimaryNodes.find(pNodeData) == mPrimaryNodes.end())
			{
				PCGNodeInfo NodeInfoData;
				NodeInfoData.pNode = pNodeData;
				NodeInfoData.IsFinished = false;
				mPrimaryNodes.insert(std::make_pair(pNodeData, NodeInfoData));
			}
		}
		else
		{
			PCGNodeInfo NodeInfoData;
			NodeInfoData.pNode = pNodeData;
			NodeInfoData.IsFinished = false;
			mNormalNodes.insert(std::make_pair(pNodeData, NodeInfoData));
		}
	}
}
