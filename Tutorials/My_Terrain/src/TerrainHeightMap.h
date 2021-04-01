#ifndef _TERRAIN_HEIGHT_MAP_H_
#define _TERRAIN_HEIGHT_MAP_H_

#pragma once

#include <string>
#include "RefCntAutoPtr.hpp"
#include "BasicMath.hpp"
#include "Texture.h"
#include "Image.h"

namespace Diligent 
{
	struct IRenderDevice;

class TerrainHeightMap
{
public:
	TerrainHeightMap();
	~TerrainHeightMap();

	void LoadHeightMap(const std::string &FileName, IRenderDevice *device);

	void GetZArea(const uint32_t& x, const uint32_t& y, const uint16_t& size, uint16_t& o_minz, uint16_t& o_maxz);

	uint16_t GetZ(const uint32_t& x, const uint32_t& y);

	ITexture* GetTexture();

public:
	uint16_t width;
	uint16_t height;
	uint16_t pitch;

private:
	RefCntAutoPtr<ITexture> m_apTex;
	RefCntAutoPtr<Image> m_apImageRawData;
};

}


#endif