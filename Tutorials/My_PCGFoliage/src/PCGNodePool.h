#ifndef _PCG_NODE_POOL_H_
#define _PCG_NODE_POOL_H_

#include <unordered_map>

namespace Diligent
{
	struct CDLODNode;
	struct SelectionInfo;

	struct PCGNodeInfo
	{
		CDLODNode *pNode;
		bool IsFinished;
	};

	struct GPUNodeData
	{
		int rx, ry, size, padding;
	};

	class PCGNodePool
	{
	public:
		PCGNodePool();
		~PCGNodePool();

		void QueryNodes(SelectionInfo *pNodeInfo);

		void GetGPUNodeData(GPUNodeData *pOutData, int Num);

	private:
		std::unordered_map<CDLODNode*, PCGNodeInfo> mPrimaryNodes;
		std::unordered_map<CDLODNode*, PCGNodeInfo> mNormalNodes;
	};
}

#endif