#ifndef _PCG_NODE_POOL_H_
#define _PCG_NODE_POOL_H_

#include <unordered_map>
#include <BasicMath.hpp>

namespace Diligent
{
	struct CDLODNode;
	struct SelectionInfo;

	struct PCGNodeData
	{
		float2 TerrainOrigin;
		float2 NodeOrigin;
		float2 CellSize;

		uint LayerIdx;
		uint MortonCode;
		uint PointNum;
		uint TexSize;

		float PlantRadius;
		float PlantZOI;
		float PCGPointGSize;
		//float Padding;

		float3 Padding;

		PCGNodeData()
		{
			TerrainOrigin = float2(0.0f);
			NodeOrigin = float2(0.0f);
			CellSize = float2(0.0f);

			LayerIdx = 0;
			MortonCode = 0;
			PointNum = 0;
			TexSize = 0;

			PlantRadius = 0.0f;
			PlantZOI = 0.0f;
			PCGPointGSize = 0.0f;
		}		
	};

	struct PCGNodeInfo
	{
		//CDLODNode *pNode;
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

		//void GetGPUNodeData(GPUNodeData **pOutData, int &Num);

	private:
		std::unordered_map<CDLODNode*, PCGNodeInfo> mPrimaryNodes;
		std::unordered_map<CDLODNode*, PCGNodeInfo> mNormalNodes;
	};
}

#endif