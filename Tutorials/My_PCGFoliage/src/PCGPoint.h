#ifndef _PCG_POINT_H_
#define _PCG_POINT_H_

#include <cstdint>
#include <array>
#include <vector>

#include "BasicMath.hpp"

namespace Diligent
{
	class PCGPoint
	{
	public:
		PCGPoint();
		PCGPoint(float radius, float2 min_c, float2 max_c, std::uint32_t seed = 0);

		~PCGPoint();

		void GeneratePoints(float radius, float2 min_c, float2 max_c, std::uint32_t seed);

	private:
		std::vector<std::array<float, 2>> mPoints;
	};
}

#endif