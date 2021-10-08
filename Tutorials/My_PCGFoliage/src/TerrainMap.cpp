#include "TerrainMap.h"

#include <assert.h>

#include "TextureLoader.h"
#include "TextureUtilities.h"
#include "RenderDevice.h"
#include "Image.h"

#include "CDLODTree.h"

namespace Diligent
{

	TerrainMap::TerrainMap() :
		width(0),
		height(0),
		pitch(0)
	{

	}


	TerrainMap::~TerrainMap()
	{

	}

	void TerrainMap::LoadMap(const std::string &DiffuseMapName, const std::string &HightMapName, IRenderDevice *device)
	{
		LoadDiffuseMap(DiffuseMapName, device);
		LoadHeightMap(HightMapName, device);
	}

	ITexture* TerrainMap::GetHeightMapTexture()
	{
		return m_apHeightTex;
	}

	ITexture* TerrainMap::GetDiffuseMapTexture()
	{
		return m_apDiffTex;
	}

	void TerrainMap::LoadHeightMap(const std::string &FileName, IRenderDevice *device)
	{
		TextureLoadInfo loadInfo;
		loadInfo.IsSRGB = false;		
		CreateTextureFromFile(FileName.c_str(), loadInfo, device, &m_apHeightTex);		
		
		//TODO: optimize interface
		CreateImageFromFile(FileName.c_str(), &m_apHeightImageRawData);

		width = m_apHeightImageRawData->GetDesc().Width;
		height = m_apHeightImageRawData->GetDesc().Height;
		
		assert(m_apHeightImageRawData->GetDesc().ComponentType == VALUE_TYPE::VT_UINT8);
		pitch = 8 * m_apHeightImageRawData->GetDesc().NumComponents;
	}

	void TerrainMap::LoadDiffuseMap(const std::string &FileName, IRenderDevice *device)
	{
		TextureLoadInfo loadInfo;
		loadInfo.IsSRGB = true;
		CreateTextureFromFile(FileName.c_str(), loadInfo, device, &m_apDiffTex);

		//TODO: optimize interface
		CreateImageFromFile(FileName.c_str(), &m_apDiffImageRawData);

		//width = m_apImageRawData->GetDesc().Width;
		//height = m_apImageRawData->GetDesc().Height;

		//assert(m_apDiffImageRawData->GetDesc().ComponentType == VALUE_TYPE::VT_UINT8);
		//pitch = 8 * m_apDiffImageRawData->GetDesc().NumComponents;
	}

	void TerrainMap::GetYArea(const uint32_t& x, const uint32_t& y, const uint16_t& size, uint16_t& o_minz, uint16_t& o_maxz)
	{
		uint8_t MinData = 255;
		uint8_t MaxData = 0;

		const uint8_t *pHeightData = reinterpret_cast<uint8_t*>(m_apHeightImageRawData->GetData()->GetDataPtr());
		int MidLength = m_apHeightImageRawData->GetDesc().Width * m_apHeightImageRawData->GetDesc().Height - 1;

		for (int j = y; j < y + size; ++j)
		{
			for (int i = x; i < x + size; ++i)
			{
				int idx = std::min(MidLength, j * width + i);
				MinData = std::min(pHeightData[idx], MinData);
				MaxData = std::max(pHeightData[idx], MaxData);
			}
		}

		o_minz = (MinData / 255.0f) * MAX_HEIGHTMAP_SIZE;
		o_maxz = (MaxData / 255.0f) * MAX_HEIGHTMAP_SIZE;
	}

	uint16_t TerrainMap::GetY(const uint32_t& x, const uint32_t& y)
	{
		const uint8_t *pHeightData = reinterpret_cast<uint8_t*>(m_apHeightImageRawData->GetData()->GetDataPtr());
		int idx = y * width + x;

		return static_cast<uint16_t>((pHeightData[idx] / 255.0f) * MAX_HEIGHTMAP_SIZE);
	}

}

