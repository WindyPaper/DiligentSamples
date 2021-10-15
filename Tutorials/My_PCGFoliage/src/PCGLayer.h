#ifndef _PCG_LAYER_H_
#define _PCG_LAYER_H_

#include <vector>

namespace Diligent
{
	enum FoliageLayerType
	{
		F_LARGE_TREE_LAYER,
		F_MEDIUM_TERR_LAYER,
		F_GRASS_LAYER,
		F_LAYER_NUM
	};

	struct PlantParam
	{
		float footprint;
		int size;
	};

	typedef std::vector<std::vector<PlantParam>> PlantParamLayer;
	class PCGLayer
	{
	public:
		PCGLayer();
		~PCGLayer();

		const PlantParamLayer &GetPlantParamLayer();

	protected:
		void SetupPlantParam();

	private:
		PlantParamLayer mLayer;
	};
}

#endif