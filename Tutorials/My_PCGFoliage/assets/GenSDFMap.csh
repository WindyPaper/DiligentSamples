
// #ifndef NUM_TEXTURES
// #   define NUM_TEXTURES 1
// #endif

// RWTexture2D<float> OutPosMapData;

Texture2D InputTexture;
Texture2D OriginalTexture;

RWTexture2D<float4> OutputTexture;

// const static int TerrainMaskTexSize = 512;
// Texture2D PosMapData; //512
//SamplerState TerrainMaskMap_sampler; // By convention, texture samplers must use the '_sampler' suffix

cbuffer cbPCGGenSDFData
{
	float2 TextureSize;
	//float2 Padding;
	float PlantRadius;
	float PlantZOI; // zone of influence
};

// cbuffer cbPCGPointDatas
// {
// 	float2 Points[262144]; //512X512	
// };

//StructuredBuffer<float2> InPCGPointDatas;

[numthreads(4, 4, 1)]
void GenSDFMain(uint3 id : SV_DispatchThreadID)
{
	//Get texture size
	uint tex_x = id.x;
	uint tex_y = id.y;

	//Get terrain size
	// float2 terrain_size = CellSize * (2 << LayerIdx);

	// float2 output_pixel_world_cell_size = CellSize / TexSize;
	// float2 global_mask_uv = ((NodeOrigin + output_pixel_world_cell_size * float2(tex_x, tex_y)) - TerrainOrigin) / terrain_size;

	// float4 global_terrain_mask_value = TerrainMaskMap.Load(int3(global_mask_uv * TerrainMaskTexSize, 0));

	float4 input_texture = InputTexture.Load(int3(id.xy, 0));
	float4 original_texture = OriginalTexture.Load(int3(id.xy, 0));

	float determin = original_texture.x;

	float distance = 0.0;
	if(determin >= 0.5f)
	{
		distance = length(id.xy - input_texture.xy * TextureSize.xy);
		float final_ret = 1.0 - saturate((distance - PlantRadius)/(PlantZOI - PlantRadius));
		OutputTexture[id.xy] = float4(final_ret, final_ret, final_ret, 1.0f);
	}
	else
	{
		distance = length(id.xy - input_texture.zw * TextureSize.xy);
		float final_ret = 1.0 - saturate((distance - PlantRadius)/(PlantZOI - PlantRadius));
		OutputTexture[id.xy] = float4(final_ret, final_ret, final_ret, 0.0f);
	}
}
