#include "TerrainHeightMap.h"

#include <assert.h>

#include "TextureLoader.h"
#include "TextureUtilities.h"
#include "RenderDevice.h"
#include "Image.h"

#include "CDLODTree.h"

namespace Diligent
{

	TerrainHeightMap::TerrainHeightMap() :
		width(0),
		height(0),
		pitch(0)
	{

	}


	TerrainHeightMap::~TerrainHeightMap()
	{

	}

	ITexture* TerrainHeightMap::GetTexture()
	{
		return m_apTex;
	}

	void TerrainHeightMap::LoadHeightMap(const std::string &FileName, IRenderDevice *device)
	{
		TextureLoadInfo loadInfo;
		loadInfo.IsSRGB = false;		
		CreateTextureFromFile(FileName.c_str(), loadInfo, device, &m_apTex);
		// Get shader resource view from the texture
		//m_TextureSRV = m_apTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

		// Set texture SRV in the SRB
		//m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
		
		//TODO: optimize interface
		CreateImageFromFile(FileName.c_str(), &m_apImageRawData);

		width = m_apImageRawData->GetDesc().Width;
		height = m_apImageRawData->GetDesc().Height;
		
		assert(m_apImageRawData->GetDesc().ComponentType == VALUE_TYPE::VT_UINT8);
		pitch = 8 * m_apImageRawData->GetDesc().NumComponents;
	}

	void TerrainHeightMap::GetZArea(const uint32_t& x, const uint32_t& y, const uint16_t& size, uint16_t& o_minz, uint16_t& o_maxz)
	{
		uint8_t MinData = 255;
		uint8_t MaxData = 0;

		const uint8_t *pHeightData = reinterpret_cast<uint8_t*>(m_apImageRawData->GetData()->GetDataPtr());

		for (int j = y; j < y + size; ++j)
		{
			for (int i = x; i < x + size; ++i)
			{
				int idx = j * width + i;
				MinData = std::min(pHeightData[idx], MinData);
				MaxData = std::max(pHeightData[idx], MaxData);
			}
		}

		o_minz = (MinData / 255.0f) * MAX_HEIGHTMAP_SIZE;
		o_maxz = (MaxData / 255.0f) * MAX_HEIGHTMAP_SIZE;
	}

	uint16_t TerrainHeightMap::GetZ(const uint32_t& x, const uint32_t& y)
	{
		const uint8_t *pHeightData = reinterpret_cast<uint8_t*>(m_apImageRawData->GetData()->GetDataPtr());
		int idx = y * width + x;

		return static_cast<uint16_t>((pHeightData[idx] / 255.0f) * MAX_HEIGHTMAP_SIZE);
	}

}

