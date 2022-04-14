
// #ifndef NUM_TEXTURES
// #   define NUM_TEXTURES 1
// #endif

RWTexture2D<float> OutPosMapData;

cbuffer cbPCGPointData
{
	float2 Origin;
	float2 Size;

	uint LayerIdx;
	uint MortonCode;
	uint PointNum;
	uint TexSize;

	float PlantRadius;
	float PlantZOI;
	float PCGPointGSize;
	float Padding;
};

// cbuffer cbPCGPointDatas
// {
// 	float2 Points[262144]; //512X512	
// };

StructuredBuffer<float2> InPCGPointDatas;

[numthreads(4, 4, 1)]
void CalculatePOSMap(uint3 id : SV_DispatchThreadID)
{
	//Get texture size
	uint tex_x = id.x;
	uint tex_y = id.y;

	float2 tex_coord = float2(tex_x, tex_y) / TexSize;
	float include_val = 0.0f;
	float2 distance_radio = (Size) / float2(PCGPointGSize, PCGPointGSize);

	for(int i = 0; i < PointNum; ++i)
	{
		float2 p = InPCGPointDatas[i] * distance_radio;

		float d = distance(tex_coord * Size, p);
		include_val += 1 - saturate((d - PlantRadius) / (PlantZOI - PlantRadius));
	}

	OutPosMapData[id.xy] = saturate(include_val);
}
