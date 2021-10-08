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
	};

	class PCGLayer
	{
	public:
		PCGLayer();
		~PCGLayer();

	private:
		std::vector<std::uint32_t> mLayer;
	};
}

#endif