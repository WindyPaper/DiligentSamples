#ifndef _LIGHT_MANAGER_H_
#define _LIGHT_MANAGER_H_

#pragma once

#include "AdvancedMath.hpp"

namespace Diligent
{
	struct DirectionalLight
	{
		float3 dir;
		float intensity;

		DirectionalLight() :
			dir(0.5f, -0.5f, 0.5f),
			intensity(1.0f)
		{}
	};

	class LightManager
	{
	public:
		LightManager();
		~LightManager();

	public:
		DirectionalLight DirLight;
	};
}

#endif