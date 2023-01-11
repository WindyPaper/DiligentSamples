#ifndef _PCG_LAYER_H_
#define _PCG_LAYER_H_

#include <vector>

namespace Diligent
{
	static const int PCG_TEX_DEFAULT_SIZE = 2048;
	static const int PCG_PLANT_MAX_POSITION_NUM = 4096 * 32;

	enum FoliageLayerType
	{
		F_LARGE_TREE_LAYER,
		F_MEDIUM_TREE_LAYER,
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

		const PlantParamLayer &GetPlantParamLayer() const;

	protected:
		void SetupPlantParam();

	private:
		PlantParamLayer mLayer;
	};
}

#endif