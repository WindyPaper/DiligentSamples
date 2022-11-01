
// #ifndef NUM_TEXTURES
// #   define NUM_TEXTURES 1
// #endif

// RWTexture2D<float> OutInputTexture;

Texture2D InputTexture;
RWTexture2D<float4> OutInitSDFMap;

// const static int TerrainMaskTexSize = 512;
// Texture2D InputTexture; //512
//SamplerState TerrainMaskMap_sampler; // By convention, texture samplers must use the '_sampler' suffix

cbuffer cbInitSDFMapData
{
	float4 TexSizeAndInvertSize; //w, h, 1/w, 1/h	
};

// cbuffer cbPCGPointDatas
// {
// 	float2 Points[262144]; //512X512	
// };

//StructuredBuffer<float2> InPCGPointDatas;

[numthreads(4, 4, 1)]
void InitSDFMapMain(uint3 id : SV_DispatchThreadID)
{
	//Get texture size
	uint tex_x = id.x;
	uint tex_y = id.y;

	//Get terrain size
	// float2 terrain_size = CellSize * (2 << LayerIdx);

	// float2 output_pixel_world_cell_size = CellSize / TexSize;
	// float2 global_mask_uv = ((NodeOrigin + output_pixel_world_cell_size * float2(tex_x, tex_y)) - TerrainOrigin) / terrain_size;

	// float4 global_terrain_mask_value = TerrainMaskMap.Load(int3(global_mask_uv * TerrainMaskTexSize, 0));

	float4 pos_val = InputTexture.Load(int3(id.xy, 0));

	float determin = pos_val.x;

	if(determin >= 0.5)
	{
		OutInitSDFMap[id.xy] = float4(-1.0, -1.0, tex_x * TexSizeAndInvertSize.z, tex_y * TexSizeAndInvertSize.w);
	}
	else
	{
		OutInitSDFMap[id.xy] = float4(tex_x * TexSizeAndInvertSize.z, tex_y * TexSizeAndInvertSize.w, -1.0, -1.0);
	}
}
