#ifndef _REFLECTION_PROBE_H_
#define _REFLECTION_PROBE_H_

#include "BasicMath.hpp"
#include "FirstPersonCamera.hpp"

namespace Diligent
{
	class ReflectionProbe
	{
	public:
		ReflectionProbe(const float3& pos);
		~ReflectionProbe();

		FirstPersonCamera *GetCameras();

	private:
		float3 m_Pos;

		FirstPersonCamera m_Cameras[6];
	};
}

#endif