
// #ifndef NUM_TEXTURES
// #   define NUM_TEXTURES 1
// #endif

RWTexture2D<float> OutPosMapData;

Texture2D PoissonPosMapData;

const static uint TerrainMaskTexSize = 512;
Texture2D TerrainMaskMap; //512

Texture2D TerrainHeightMap;

//SamplerState TerrainMaskMap_sampler; // By convention, texture samplers must use the '_sampler' suffix

#ifndef NUM_DENSITY_TEXTURES
#   define NUM_DENSITY_TEXTURES 1
#endif

Texture2D DensityTextures[NUM_DENSITY_TEXTURES];
SamplerState DensityTextures_sampler; // By convention, texture samplers must use the '_sampler' suffix

const static uint PCG_PLANT_MAX_POSITION_NUM = 4096 * 32;
RWBuffer<uint> PlantTypeNumBuffer;
RWBuffer<float4> PlantPositionBuffers;

cbuffer cbPCGPointData
{
	float2 TerrainOrigin;
	float2 NodeOrigin;
	float2 CellSize;

	uint LayerIdx;
	uint MortonCode;
	uint PointNum;
	uint TexSize;

	float PlantRadius;
	float PlantZOI;
	float PCGPointGSize;

	float2 TexSampOffsetInParent;

	float PlantPlaceThreshold;
};

// cbuffer cbPCGPointDatas
// {
// 	float2 Points[262144]; //512X512	
// };

//StructuredBuffer<float2> InPCGPointDatas;

uint GetLinearQuadIndex(const uint Layer, const uint MortonCode)
{
	uint offset = 0;
	for (uint i = 0; i < Layer; ++i)
	{
		offset += pow(4, i + 1);
	}

	return offset + MortonCode;
}

uint GetParentIndex(const uint LinearQuadIndex)
{
	uint parent_index = (LinearQuadIndex >> 2) - 1;

	return parent_index;
}

uint GetLayerFstElementIdx(const uint Layer)
{
	uint offset = 0;
	for (uint i = 0; i < Layer; ++i)
	{
		offset += pow(4, i + 1);
	}

	return offset;
}

[numthreads(4, 4, 1)]
void CalculatePOSMap(uint3 id : SV_DispatchThreadID)
{
	//Get texture size
	uint tex_x = id.x;
	uint tex_y = id.y;

	//Get terrain size
	float2 terrain_size = CellSize * (2 << LayerIdx);

	float2 output_pixel_world_cell_size = CellSize / TexSize;
	float2 global_mask_uv = ((NodeOrigin + output_pixel_world_cell_size * float2(tex_x, tex_y)) - TerrainOrigin) / terrain_size;

	float4 global_terrain_mask_value = TerrainMaskMap.Load(int3(global_mask_uv * TerrainMaskTexSize, 0));

	//OutPosMapData[id.xy] = PoissonPosMapData.Load(int3(id.xy, 0)) * global_terrain_mask_value.r;

	float P = 1.0f;

	float4 poisson_pos = PoissonPosMapData.Load(int3(id.xy, 0)) * global_terrain_mask_value.r;

	P = poisson_pos.r;

	if(LayerIdx > 0 && poisson_pos.r > 0.0f) //evaluate pos from last layer sdf
	{
		uint SampleLinearQuadIdx = GetLinearQuadIndex(LayerIdx, MortonCode);
		for(uint SampleLayerId = LayerIdx; SampleLayerId > 0; --SampleLayerId)
		{
			uint parent_tex_index = GetParentIndex(SampleLinearQuadIdx);
		
			int2 parent_tex_coord = int2(tex_x, tex_y) + int2(TexSampOffsetInParent);

			if(SampleLayerId != LayerIdx)
			{
				uint layer_fst_element_idx = GetLayerFstElementIdx(SampleLayerId);
				uint quad_idx = parent_tex_index - layer_fst_element_idx;
				uint half_parent_tex_size = TexSize << ((LayerIdx - SampleLayerId) - 1);

				parent_tex_coord += uint2(saturate(quad_idx & 1u), saturate(quad_idx & 2u)) * half_parent_tex_size;
			}

			float4 parent_sdf_tex = DensityTextures[parent_tex_index].Load(int3(parent_tex_coord, 0));

			P *= (1.0f - parent_sdf_tex.r);

			SampleLinearQuadIdx = parent_tex_index;
		}		
	}

	if(P > PlantPlaceThreshold)
	{		
		OutPosMapData[id.xy] = 1.0f;

		uint curr_idx = 0;
		InterlockedAdd(PlantTypeNumBuffer[LayerIdx], 1u, curr_idx);

		//plant position buffer index
		uint plant_pos_buff_idx = LayerIdx * PCG_PLANT_MAX_POSITION_NUM + curr_idx;
		float2 xz_pos = terrain_size * global_mask_uv + TerrainOrigin;
		PlantPositionBuffers[plant_pos_buff_idx] = float4(xz_pos.r, 1.0f, xz_pos.g, 1.0f);
	}
	else
	{
		OutPosMapData[id.xy] = 0.0f;
	}
}
