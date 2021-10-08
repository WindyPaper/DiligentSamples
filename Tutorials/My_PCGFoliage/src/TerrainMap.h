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

class TerrainMap
{
public:
	TerrainMap();
	~TerrainMap();

	void LoadMap(const std::string &DiffuseMapName, const std::string &HightMapName, IRenderDevice *device);	

	void GetYArea(const uint32_t& x, const uint32_t& y, const uint16_t& size, uint16_t& o_minz, uint16_t& o_maxz);

	uint16_t GetY(const uint32_t& x, const uint32_t& y);

	ITexture* GetHeightMapTexture();
	ITexture* GetDiffuseMapTexture();

public:
	uint16_t width;
	uint16_t height;
	uint16_t pitch;

protected:
	void LoadHeightMap(const std::string &FileName, IRenderDevice *device);
	void LoadDiffuseMap(const std::string &FileName, IRenderDevice *device);

private:
	RefCntAutoPtr<ITexture> m_apHeightTex;
	RefCntAutoPtr<Image> m_apHeightImageRawData;

	RefCntAutoPtr<ITexture> m_apDiffTex;
	RefCntAutoPtr<Image> m_apDiffImageRawData;
};

}


#endif