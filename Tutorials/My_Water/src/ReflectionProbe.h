#ifndef _REFLECTION_PROBE_H_
#define _REFLECTION_PROBE_H_

#include "BasicMath.hpp"

namespace Diligent
{
	class ReflectionProbe
	{
	public:
		ReflectionProbe(const float3& pos);
		~ReflectionProbe();

	private:
		float3 m_Pos;
	};
}

#endif