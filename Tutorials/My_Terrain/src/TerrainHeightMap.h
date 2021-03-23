#ifndef _TERRAIN_HEIGHT_MAP_H_
#define _TERRAIN_HEIGHT_MAP_H_

#pragma once

#include <string>
#include "RefCntAutoPtr.hpp"
#include "BasicMath.hpp"
#include "Texture.h"

namespace Diligent 
{
	struct IRenderDevice;

class TerrainHeightMap
{
public:
	TerrainHeightMap();
	~TerrainHeightMap();

	void UpdateLevel(const float2 &pos);

	void LoadHeightMap(const std::string &FileName, IRenderDevice *device);

	ITexture* GetTexture();

private:
	RefCntAutoPtr<ITexture> m_apTex;
};

}


#endif