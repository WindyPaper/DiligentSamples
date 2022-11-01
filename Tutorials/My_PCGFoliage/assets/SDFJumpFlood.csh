
// #ifndef NUM_TEXTURES
// #   define NUM_TEXTURES 1
// #endif

// RWTexture2D<float> OutPosMapData;

Texture2D InputTexture;
RWTexture2D<float4> OutputTexture;

// const static int TerrainMaskTexSize = 512;
// Texture2D PosMapData; //512
//SamplerState TerrainMaskMap_sampler; // By convention, texture samplers must use the '_sampler' suffix

const static int2 directions[] =
{
	int2(-1, -1),
	int2(-1, 0),
	int2(-1, 1),
	int2(0, -1),
	int2(0, 1),
	int2(1, -1),
	int2(1, 0),
	int2(1, 1)
};

cbuffer cbPCGSDFJumpFloodData
{
	float2 Step;
	float2 TextureSize;
};

// cbuffer cbPCGPointDatas
// {
// 	float2 Points[262144]; //512X512	
// };

//StructuredBuffer<float2> InPCGPointDatas;

float4 JFAOutside(float4 inputTex, float2 idxy)
{
	float4 outputTex = inputTex;

	//cull inside
	if (inputTex.x != -1)
	{
		float2 nearestUV = inputTex.zw;
		float minDistance = 1e16;

		//if had min distance in previous flooding
		if (inputTex.z != -1)
		{
			minDistance = length(idxy - nearestUV * TextureSize.xy);
		}


		bool hasMin = false;
		for (uint i = 0; i < 8; i++)
		{
			uint2 sampleOffset = idxy + directions[i] * Step;
			sampleOffset = clamp(sampleOffset, 0, TextureSize.xy - 1);
			float4 offsetTexture = InputTexture.Load(float3(sampleOffset, 0));
			//if had min distance in previous flooding
			if (offsetTexture.z != -1)
			{
				float2 tempUV = offsetTexture.zw;
				float tempDistance = length(idxy - tempUV * TextureSize.xy);
				if (tempDistance < minDistance)
				{
					hasMin = true;
					minDistance = tempDistance;
					nearestUV = tempUV;
				}
			}
		}

		if (hasMin)
		{
			outputTex = float4(inputTex.xy, nearestUV);
		}
	}
	return outputTex;
}

float4 JFAInside(float4 inputTex, float2 idxy)
{
	float4 outputTex = inputTex;

	//cull outside
	if (inputTex.z != -1)
	{
		float2 nearestUV = inputTex.xy;
		float minDistance = 1e16;

		//if had min distance in previous flooding
		if (inputTex.x != -1)
		{
			minDistance = length(idxy - nearestUV * TextureSize.xy);
		}


		bool hasMin = false;
		for (uint i = 0; i < 8; i++)
		{
			uint2 sampleOffset = idxy + directions[i] * Step;
			sampleOffset = clamp(sampleOffset, 0, TextureSize.xy - 1);
			float4 offsetTexture = InputTexture.Load(float3(sampleOffset, 0));
			//if had min distance in previous flooding
			if (offsetTexture.x != -1)
			{
				float2 tempUV = offsetTexture.xy;
				float tempDistance = length(idxy - tempUV * TextureSize.xy);
				if (tempDistance < minDistance)
				{
					hasMin = true;
					minDistance = tempDistance;
					nearestUV = tempUV;
				}
			}
		}

		if (hasMin)
		{
			outputTex = float4(nearestUV, inputTex.zw);
		}
	}
	return outputTex;
}

[numthreads(4, 4, 1)]
void SDFJumpFloodMain(uint3 id : SV_DispatchThreadID)
{
	//Get texture size
	uint tex_x = id.x;
	uint tex_y = id.y;

	//Get terrain size
	// float2 terrain_size = CellSize * (2 << LayerIdx);

	// float2 output_pixel_world_cell_size = CellSize / TexSize;
	// float2 global_mask_uv = ((NodeOrigin + output_pixel_world_cell_size * float2(tex_x, tex_y)) - TerrainOrigin) / terrain_size;

	// float4 global_terrain_mask_value = TerrainMaskMap.Load(int3(global_mask_uv * TerrainMaskTexSize, 0));

	float4 inputTexture = InputTexture.Load(float3(id.xy, 0));
	float4 outSide = JFAOutside(inputTexture, id.xy);
	OutputTexture[id.xy] = JFAInside(outSide, id.xy);
}
