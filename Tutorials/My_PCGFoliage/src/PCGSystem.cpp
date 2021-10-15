#include "PCGSystem.h"
#include "CDLODTree.h"

Diligent::PCGSystem::PCGSystem(IDeviceContext *pContext, IRenderDevice *pDevice) :
	m_pContext(pContext),
	m_pRenderDevice(pDevice),
	mSeed(0)
{
	CreatePoissonDiskSamplerData();
}

Diligent::PCGSystem::~PCGSystem()
{

}

void Diligent::PCGSystem::CreatePoissonDiskSamplerData()
{
	mPointVec.resize(F_LAYER_NUM);
	const PlantParamLayer &PlantLayer = mPlantLayer.GetPlantParamLayer();

	for (int i = 0; i < PlantLayer[F_LARGE_TREE_LAYER].size(); ++i)
	{
		const PlantParam &param = PlantLayer[F_LARGE_TREE_LAYER][i];

		std::vector<std::array<float, 2>> RetPoints;
		float kRadius = param.footprint;
		float2 kXMin = float2(0.F + kRadius, 0.F + kRadius); //avoid overlap
		float2 kXMax = float2(param.size - kRadius, param.size - kRadius); //avoid overlap

		//RetPoints = thinks::PoissonDiskSampling(kRadius, kXMin, kXMax);
		mPointVec[i].GeneratePoints(kRadius, kXMin, kXMax, mSeed);
	}
}

void Diligent::PCGSystem::DoProcedural(SelectionInfo *pNodeInfo)
{
	mNodePool.QueryNodes(pNodeInfo);
}
