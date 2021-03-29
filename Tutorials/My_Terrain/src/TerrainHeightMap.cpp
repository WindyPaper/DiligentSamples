#include "TerrainHeightMap.h"
#include "TextureLoader.h"
#include "TextureUtilities.h"
#include "RenderDevice.h"
#include "Image.h"

namespace Diligent
{

	TerrainHeightMap::TerrainHeightMap()
	{

	}


	TerrainHeightMap::~TerrainHeightMap()
	{

	}

	void TerrainHeightMap::UpdateLevel(const float2 &pos)
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

	}

}

