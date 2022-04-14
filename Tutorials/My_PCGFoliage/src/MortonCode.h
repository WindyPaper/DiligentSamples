#ifndef _MORTON_CODE_H_
#define _MORTON_CODE_H_

#include <stdint.h>

namespace Diligent
{
	class MortonCode
	{
	public:
		MortonCode() {};

		uint32_t Morton2D(uint16_t x, uint16_t y)
		{
			uint32_t x32 = static_cast<uint32_t>(x);
			uint32_t y32 = static_cast<uint32_t>(y);

			return partition(x32) | (partition(y32) << 1);
		}

		inline uint32_t partition(uint32_t n)
		{
			// n = ----------------fedcba9876543210 : Bits initially
			// n = --------fedcba98--------76543210 : After (1)
			// n = ----fedc----ba98----7654----3210 : After (2)
			// n = --fe--dc--ba--98--76--54--32--10 : After (3)
			// n = -f-e-d-c-b-a-9-8-7-6-5-4-3-2-1-0 : After (4)
			n = (n ^ (n << 8)) & 0x00ff00ff; // (1)
			n = (n ^ (n << 4)) & 0x0f0f0f0f; // (2)
			n = (n ^ (n << 2)) & 0x33333333; // (3)
			return (n ^ (n << 1)) & 0x55555555; // (4)				
		}
	};
}

#endif