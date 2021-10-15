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

const Diligent::PlantParamLayer & Diligent::PCGLayer::GetPlantParamLayer()
{
	return mLayer;
}

void Diligent::PCGLayer::SetupPlantParam()
{
	/*PlantParam TestLargeTree;
	TestLargeTree.footprint = 0.3f;
	TestLargeTree.size = LEAF_RENDER_NODE_SIZE * 2;

	mLayer[F_LARGE_TREE_LAYER].push_back(TestLargeTree);*/

	PlantParam TestMediumTree;
	TestMediumTree.footprint = 0.1f;
	TestMediumTree.size = LEAF_RENDER_NODE_SIZE;

	mLayer[F_MEDIUM_TERR_LAYER].push_back(TestMediumTree);
}
