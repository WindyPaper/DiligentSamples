
// #ifndef NUM_TEXTURES
// #   define NUM_TEXTURES 1
// #endif

RWTexture2D<float> OutPosMapData;

Texture2D PoissonPosMapData;

const static int TerrainMaskTexSize = 512;
Texture2D TerrainMaskMap; //512
//SamplerState TerrainMaskMap_sampler; // By convention, texture samplers must use the '_sampler' suffix

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
	float3 Padding;
};

// cbuffer cbPCGPointDatas
// {
// 	float2 Points[262144]; //512X512	
// };

//StructuredBuffer<float2> InPCGPointDatas;

[numthreads(4, 4, 1)]
void GenSDFMapMain(uint3 id : SV_DispatchThreadID)
{
	//Get texture size
	uint tex_x = id.x;
	uint tex_y = id.y;

	//Get terrain size
	float2 terrain_size = CellSize * (2 << LayerIdx);

	float2 output_pixel_world_cell_size = CellSize / TexSize;
	float2 global_mask_uv = ((NodeOrigin + output_pixel_world_cell_size * float2(tex_x, tex_y)) - TerrainOrigin) / terrain_size;

	float4 global_terrain_mask_value = TerrainMaskMap.Load(int3(global_mask_uv * TerrainMaskTexSize, 0));

	OutPosMapData[id.xy] = PoissonPosMapData.Load(int3(id.xy, 0)) * global_terrain_mask_value.r;
}
