#include "PCGLayer.h"
#include "CDLODTree.h"

Diligent::PCGLayer::PCGLayer()
{
	mLayer.resize(F_LAYER_NUM);

	SetupPlantParam();
}

Diligent::PCGLayer::~PCGLayer()
{

}

const Diligent::PlantParamLayer & Diligent::PCGLayer::GetPlantParamLayer() const
{
	return mLayer;
}

void Diligent::PCGLayer::SetupPlantParam()
{
	for (int i = 0; i < F_LAYER_NUM; ++i)
	{
		PlantParam pt;

		pt.size = PCG_TEX_DEFAULT_SIZE >> i;

		switch (i)
		{
		case F_LARGE_TREE_LAYER:
			pt.footprint = 0.8f;
			break;

		case F_MEDIUM_TREE_LAYER:
			pt.footprint = 0.5f;
			break;

		case F_GRASS_LAYER:
			pt.footprint = 0.1f;
			break;

		default:
			break;
		}

		mLayer[i].push_back(pt);
	}
}
