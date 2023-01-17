#include "PCGSystem.h"
#include "CDLODTree.h"

#include "RenderDevice.h"

#include "TextureLoader.h"
#include "TextureUtilities.h"
#include "MapHelper.hpp"

Diligent::PCGSystem::PCGSystem(IDeviceContext *pContext, IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory, const Dimension &TerrainDim) :
	m_pContext(pContext),
	m_pRenderDevice(pDevice),
	mSeed(0),
	mTerrainDim(TerrainDim),
	mPCGCSCall(pDevice, pShaderFactory)
{
	Init();
}

Diligent::PCGSystem::~PCGSystem()
{
	delete mTerrainTile;
	mTerrainTile = nullptr;
}

void Diligent::PCGSystem::Init()
{
	float2 tile_min = float2(mTerrainDim.Min.x, mTerrainDim.Min.z);
	float2 tile_size = float2(mTerrainDim.Size.x, mTerrainDim.Size.z);
	mTerrainTile = new PCGTerrainTile(m_pContext, m_pRenderDevice, tile_min, tile_size);

	//CreatePoissonDiskSamplerData();	
}

void Diligent::PCGSystem::CreatePoissonDiskSamplerData()
{
	mPointVec.resize(F_LAYER_NUM);
	const PlantParamLayer &PlantLayer = mPlantLayer.GetPlantParamLayer();

	for (int i = 0; i < F_LAYER_NUM; ++i)
	{
		const PlantParam &param = PlantLayer[i][0];

		std::vector<std::array<float, 2>> RetPoints;
		float kRadius = param.footprint;
		float2 kXMin = float2(0.F + kRadius, 0.F + kRadius); //avoid overlap
		float2 kXMax = float2(param.size - kRadius, param.size - kRadius); //avoid overlap

		//RetPoints = thinks::PoissonDiskSampling(kRadius, kXMin, kXMax);
		//mPointVec[i].GeneratePoints(kRadius, kXMin, kXMax, mSeed);
		mPointVec[i].ReadPoints(i);
	}

	//To device buffer
	mPCGCSCall.CreateGlobalPointBuffer(mPointVec);
}

void Diligent::PCGSystem::DoProcedural()
{
	//mNodePool.QueryNodes(pNodeInfo);
	mTerrainTile->GenerateNodes(&mPlantLayer, mPointVec);
	mTerrainTile->GeneratePosMap(&mPCGCSCall);

	//mTerrainTile->GenerateSDFMap(&mPCGCSCall);
}

Diligent::PCGResultData Diligent::PCGSystem::GetPCGResultData()
{
	return mTerrainTile->GetPCGResultData();
}

void Diligent::PCGTerrainTile::CreateSpecificPCGTexture(const uint32_t Layer, const uint32_t LinearQuadIndex)
{
	TextureDesc DensityTexType;
	DensityTexType.Type = RESOURCE_DIM_TEX_2D;
	DensityTexType.Width = PCG_TEX_DEFAULT_SIZE >> Layer;
	DensityTexType.Height = DensityTexType.Width;
	DensityTexType.MipLevels = 1;
	DensityTexType.Format = TEX_FORMAT_R8_UNORM;
	DensityTexType.Usage = USAGE_DYNAMIC;
	DensityTexType.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	m_pRenderDevice->CreateTexture(DensityTexType, nullptr, &mGPUDensityTexArray[LinearQuadIndex]);
	m_pRenderDevice->CreateTexture(DensityTexType, nullptr, &mGPUSDFResultTexArray[LinearQuadIndex]);

	TextureDesc SDFTexType;
	SDFTexType.Type = RESOURCE_DIM_TEX_2D;
	SDFTexType.Width = PCG_TEX_DEFAULT_SIZE >> Layer;
	SDFTexType.Height = SDFTexType.Width;
	SDFTexType.MipLevels = 1;
	SDFTexType.Format = TEX_FORMAT_RGBA32_FLOAT;
	SDFTexType.Usage = USAGE_DYNAMIC;
	SDFTexType.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	m_pRenderDevice->CreateTexture(SDFTexType, nullptr, &mGPUSDFTexArrayPing[LinearQuadIndex]);
	m_pRenderDevice->CreateTexture(SDFTexType, nullptr, &mGPUSDFTexArrayPong[LinearQuadIndex]);
}

void Diligent::PCGTerrainTile::CreatePCGNodeDataBuffer(const std::vector<PCGNodeData> &PCGNodeDataVec)
{
	/*mPCGGPUNodeBufferVec.resize(PCGNodeDataVec.size());

	for (int i = 0; i < PCGNodeDataVec.size(); ++i)
	{
		BufferDesc BuffDesc;
		BuffDesc.Name = "PCG Node Data buffer";
		BuffDesc.Usage = USAGE_DEFAULT;
		BuffDesc.BindFlags = BIND_SHADER_RESOURCE;
		BuffDesc.Mode = BUFFER_MODE_STRUCTURED;
		BuffDesc.ElementByteStride = sizeof(PCGNodeData);
		BuffDesc.uiSizeInBytes = sizeof(PCGNodeData);

		BufferData VBData;
		VBData.pData = &PCGNodeDataVec[i];
		VBData.DataSize = sizeof(PCGNodeData);
		m_pRenderDevice->CreateBuffer(BuffDesc, &VBData, &mPCGGPUNodeBufferVec[i]);
	}*/

	BufferDesc BuffDesc;
	BuffDesc.Name = "PCG Node Data buffer";
	BuffDesc.Usage = USAGE_DYNAMIC;
	BuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
	BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
	BuffDesc.uiSizeInBytes = sizeof(PCGNodeData);
	m_pRenderDevice->CreateBuffer(BuffDesc, nullptr, &mPCGGPUNodeConstBuffer);

	//init position buffer
	BufferDesc PlantInitPosBuffDesc;
	PlantInitPosBuffDesc.Name = "PCG init Plant Positions buffer";
	PlantInitPosBuffDesc.Usage = USAGE_DEFAULT;
	PlantInitPosBuffDesc.ElementByteStride = sizeof(float4);
	PlantInitPosBuffDesc.Mode = BUFFER_MODE_FORMATTED;
	PlantInitPosBuffDesc.uiSizeInBytes = PlantInitPosBuffDesc.ElementByteStride * PCG_PLANT_MAX_POSITION_NUM * F_LAYER_NUM;
	PlantInitPosBuffDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	m_pRenderDevice->CreateBuffer(PlantInitPosBuffDesc, nullptr, &mPlantPositionBuffers);

	//init layer plant type buffer
	BufferDesc LayerPlantTypeBuffDesc;
	LayerPlantTypeBuffDesc.Name = "PCG init layer plant type buffer";
	LayerPlantTypeBuffDesc.Usage = USAGE_DEFAULT;
	LayerPlantTypeBuffDesc.ElementByteStride = sizeof(uint32_t);
	LayerPlantTypeBuffDesc.Mode = BUFFER_MODE_FORMATTED;
	LayerPlantTypeBuffDesc.uiSizeInBytes = LayerPlantTypeBuffDesc.ElementByteStride * F_LAYER_NUM;
	LayerPlantTypeBuffDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	m_pRenderDevice->CreateBuffer(LayerPlantTypeBuffDesc, nullptr, &mPlantTypeNumBuffer);

	//init stage buffer
	BufferDesc PlantPosStageBufferDesc;
	PlantPosStageBufferDesc.Name = "Plant position staging buffer";
	PlantPosStageBufferDesc.Usage = USAGE_STAGING;
	PlantPosStageBufferDesc.BindFlags = BIND_NONE;
	PlantPosStageBufferDesc.Mode = BUFFER_MODE_UNDEFINED;
	PlantPosStageBufferDesc.CPUAccessFlags = CPU_ACCESS_READ;
	PlantPosStageBufferDesc.uiSizeInBytes = sizeof(float4) * PCG_PLANT_MAX_POSITION_NUM * F_LAYER_NUM;
	PlantPosStageBufferDesc.ElementByteStride = sizeof(float4);
	m_pRenderDevice->CreateBuffer(PlantPosStageBufferDesc, nullptr, &mPlantPosStageDatas);

	BufferDesc PlantTypeNumStageBufferDesc;
	PlantTypeNumStageBufferDesc.Name = "Plant type num staging buffer";
	PlantTypeNumStageBufferDesc.Usage = USAGE_STAGING;
	PlantTypeNumStageBufferDesc.BindFlags = BIND_NONE;
	PlantTypeNumStageBufferDesc.Mode = BUFFER_MODE_UNDEFINED;
	PlantTypeNumStageBufferDesc.CPUAccessFlags = CPU_ACCESS_READ;
	PlantTypeNumStageBufferDesc.uiSizeInBytes = sizeof(uint32_t) * F_LAYER_NUM;
	PlantTypeNumStageBufferDesc.ElementByteStride = sizeof(uint32_t);
	m_pRenderDevice->CreateBuffer(PlantTypeNumStageBufferDesc, nullptr, &mPlantTypeNumStageData);

	//VERIFY_EXPR(mPlantTypeNumStageData != nullptr);

	FenceDesc FDesc;
	FDesc.Name = "plant stage buffer available";
	m_pRenderDevice->CreateFence(FDesc, &mPlantStageDataAvailable);
}

uint32_t Diligent::PCGTerrainTile::GetLinearQuadIndex(const uint32_t Layer, const uint32_t MortonCode)
{
	uint32_t offset = 0;
	for (uint32_t i = 0; i < Layer; ++i)
	{
		offset += static_cast<uint32_t>(std::pow(4, i + 1));
	}

	return offset + MortonCode;
}

uint32_t Diligent::PCGTerrainTile::GetParentIndex(const uint32_t LinearQuadIndex)
{
	uint32_t parent_index = (LinearQuadIndex >> 2) - 1;

	return parent_index;
}

void Diligent::PCGTerrainTile::DivideTile(const PCGLayer *pLayer, const std::vector<PCGPoint> &PointVec)
{
	//mPCGNodeDataVec.emplace_back(PCGNodeData());
	uint32_t texNum = GetPCGTextureNum();

	mPCGNodeDataVec.resize(texNum);
	mGPUDensityTexArray.resize(texNum);
	mGPUSDFResultTexArray.resize(texNum);
	mGPUSDFTexArrayPing.resize(texNum);
	mGPUSDFTexArrayPong.resize(texNum);

	float width = mTileSize.x;
	float height = mTileSize.y;
	//float2 terrainOrigin = float2(mTerrainDim.MinX, mTerrainDim.MinZ);
	
	for (int i = 0; i < F_LAYER_NUM; ++i)
	{
		uint16_t xDivideNum = 2 << i;
		uint16_t yDivideNum = xDivideNum;

		float cellWidth = width / xDivideNum;
		float cellHeight = height / yDivideNum;

		for (uint16_t y = 0; y < yDivideNum; ++y)
		{
			for (uint16_t x = 0; x < xDivideNum; ++x)
			{
				uint32_t MortonVal = mMortonCode.Morton2D(x, y);

				uint32_t LinearArrayIdx = GetLinearQuadIndex(i, MortonVal);

				PCGNodeData pcgData = PCGNodeData();
				pcgData.LayerIdx = i;
				pcgData.MortonCode = MortonVal;
				pcgData.TerrainOrigin = mTileMin;
				pcgData.NodeOrigin = float2(x * cellWidth, y * cellHeight) + mTileMin;
				pcgData.CellSize = float2(cellWidth, cellHeight);
				pcgData.PointNum = 0;// PointVec[i].GetNum();
				pcgData.TexSize = PCG_TEX_DEFAULT_SIZE >> i;
				pcgData.PlantRadius = pLayer->GetPlantParamLayer()[i][0].footprint;
				pcgData.PlantZOI = pcgData.PlantRadius + pLayer->GetPlantParamLayer()[i][0].footprint / 2.0f;
				pcgData.PCGPointGSize = pLayer->GetPlantParamLayer()[i][0].size;

				//get parent pcg node data
				if (i > 0)
				{
					uint32_t parent_linear_index = GetParentIndex(LinearArrayIdx);
					const PCGNodeData &parentPCGNodeData = mPCGNodeDataVec[parent_linear_index];
					pcgData.TexSampOffsetInParent = abs(parentPCGNodeData.NodeOrigin - pcgData.NodeOrigin) * float2(parentPCGNodeData.TexSize) / parentPCGNodeData.CellSize;
				}

				mPCGNodeDataVec[LinearArrayIdx] = pcgData;

				CreateSpecificPCGTexture(i, LinearArrayIdx);
			}
		}
	}

	CreatePCGNodeDataBuffer(mPCGNodeDataVec);
}

Diligent::PCGTerrainTile::PCGTerrainTile(IDeviceContext *pContext, IRenderDevice *pDevice, const float2 &min, const float2 &size) :
	m_pContext(pContext),
	m_pRenderDevice(pDevice),
	mTileMin(min),
	mTileSize(size)
{
	InitGlobalRes();
}

void Diligent::PCGTerrainTile::InitGlobalRes()
{
	TextureLoadInfo loadInfo;
	loadInfo.IsSRGB = false;
	CreateTextureFromFile("./PCGRoadMask.png", loadInfo, m_pRenderDevice, &mGlobalTerrainMaskTex);
}

void Diligent::PCGTerrainTile::GenerateNodes(const PCGLayer *pLayer, const std::vector<PCGPoint> &PointVec)
{
	DivideTile(pLayer, PointVec);
}

void Diligent::PCGTerrainTile::GeneratePosMap(PCGCSCall *pPCGCall)
{
	uint lastLayer = F_LAYER_NUM;
	for (int i = 0; i < mGPUDensityTexArray.size(); ++i)
	{
		pPCGCall->PosMapSetPSO(m_pContext);
		pPCGCall->BindTerrainMaskMap(m_pContext, mGlobalTerrainMaskTex);

		uint currLayerIdx = mPCGNodeDataVec[i].LayerIdx;
		if (lastLayer != currLayerIdx)
		{
			pPCGCall->BindPoissonPosMap(currLayerIdx);
			lastLayer = currLayerIdx;
		}
		
		pPCGCall->BindPosMapRes(m_pContext, mPCGGPUNodeConstBuffer, mPCGNodeDataVec[i], mGPUDensityTexArray[i], mGPUSDFResultTexArray);
		pPCGCall->BindPosBuffer(m_pContext, mPlantTypeNumBuffer, mPlantPositionBuffers);

		uint mapSize = PCG_TEX_DEFAULT_SIZE >> currLayerIdx;
		pPCGCall->PosMapDispatch(m_pContext, mapSize);

		//Generate density map to evaluate pos data
		GenerateSDFMap(pPCGCall, i);
	}

	ReadBackPositionDataToHost();
}

//void Diligent::PCGTerrainTile::GenerateSDFMap(PCGCSCall *pPCGCall)
//{
//	for (int i = 0; i < mGPUDensityTexArray.size(); ++i)
//	{
//		const PCGNodeData &nodeData = mPCGNodeDataVec[i];
//
//		//init sdf map
//		pPCGCall->InitSDFMapSetPSO(m_pContext);
//		pPCGCall->BindInitSDFMapData(m_pContext, nodeData, mGPUDensityTexArray[i], mGPUSDFTexArrayPing[i]);
//		pPCGCall->InitSDFDispatch(m_pContext, nodeData.TexSize);
//
//		//sdf jump flood
//		bool reverse_val = false;
//		int2 step = int2((mPCGNodeDataVec[i].TexSize + 1) >> 1, (mPCGNodeDataVec[i].TexSize + 1) >> 1);
//		pPCGCall->SDFJumpFloodSetPSO(m_pContext);
//		while (step.x > 1 || step.y > 1)
//		{			
//			pPCGCall->BindSDFJumpFloodData(m_pContext, nodeData, float2(step.x, step.y), mGPUSDFTexArrayPing[i], mGPUSDFTexArrayPong[i], reverse_val);			
//
//			reverse_val = !reverse_val;
//			step = int2((step.x + 1) >> 1, (step.y + 1) >> 1);
//			pPCGCall->SDFJumpFloodDispatch(m_pContext, nodeData.TexSize);
//		}
//		pPCGCall->BindSDFJumpFloodData(m_pContext, nodeData, float2(1.0f, 1.0f), mGPUSDFTexArrayPing[i], mGPUSDFTexArrayPong[i], reverse_val);
//		pPCGCall->SDFJumpFloodDispatch(m_pContext, nodeData.TexSize);
//		reverse_val = !reverse_val;
//		pPCGCall->BindSDFJumpFloodData(m_pContext, nodeData, float2(1.0f, 1.0f), mGPUSDFTexArrayPing[i], mGPUSDFTexArrayPong[i], reverse_val);
//		pPCGCall->SDFJumpFloodDispatch(m_pContext, nodeData.TexSize);
//		reverse_val = !reverse_val;		
//
//		//gen sdf composition
//		pPCGCall->GenSDFMapSetPSO(m_pContext);
//		pPCGCall->BindGenSDFMapData(m_pContext, nodeData, mGPUDensityTexArray[i], mGPUSDFTexArrayPing[i], mGPUSDFTexArrayPong[i], reverse_val);
//		pPCGCall->GenSDFMapDispatch(m_pContext, nodeData.TexSize);
//
//		reverse_val = !reverse_val;
//	}
//
//}

void Diligent::PCGTerrainTile::GenerateSDFMap(PCGCSCall *pPCGCall, int index)
{
	int i = index;
	const PCGNodeData &nodeData = mPCGNodeDataVec[i];

	//init sdf map
	pPCGCall->InitSDFMapSetPSO(m_pContext);
	pPCGCall->BindInitSDFMapData(m_pContext, nodeData, mGPUDensityTexArray[i], mGPUSDFTexArrayPing[i]);
	pPCGCall->InitSDFDispatch(m_pContext, nodeData.TexSize);

	//sdf jump flood
	bool reverse_val = false;
	int2 step = int2((mPCGNodeDataVec[i].TexSize + 1) >> 1, (mPCGNodeDataVec[i].TexSize + 1) >> 1);
	pPCGCall->SDFJumpFloodSetPSO(m_pContext);
	while (step.x > 1 || step.y > 1)
	{
		pPCGCall->BindSDFJumpFloodData(m_pContext, nodeData, float2(step.x, step.y), mGPUSDFTexArrayPing[i], mGPUSDFTexArrayPong[i], reverse_val);

		reverse_val = !reverse_val;
		step = int2((step.x + 1) >> 1, (step.y + 1) >> 1);
		pPCGCall->SDFJumpFloodDispatch(m_pContext, nodeData.TexSize);
	}
	pPCGCall->BindSDFJumpFloodData(m_pContext, nodeData, float2(1.0f, 1.0f), mGPUSDFTexArrayPing[i], mGPUSDFTexArrayPong[i], reverse_val);
	pPCGCall->SDFJumpFloodDispatch(m_pContext, nodeData.TexSize);
	reverse_val = !reverse_val;
	pPCGCall->BindSDFJumpFloodData(m_pContext, nodeData, float2(1.0f, 1.0f), mGPUSDFTexArrayPing[i], mGPUSDFTexArrayPong[i], reverse_val);
	pPCGCall->SDFJumpFloodDispatch(m_pContext, nodeData.TexSize);
	reverse_val = !reverse_val;

	//gen sdf composition
	pPCGCall->GenSDFMapSetPSO(m_pContext);
	if (!reverse_val)
	{
		pPCGCall->BindGenSDFMapData(m_pContext, nodeData, mGPUDensityTexArray[i], mGPUSDFTexArrayPing[i], mGPUSDFResultTexArray[i], reverse_val);
	}
	else
	{
		pPCGCall->BindGenSDFMapData(m_pContext, nodeData, mGPUDensityTexArray[i], mGPUSDFTexArrayPong[i], mGPUSDFResultTexArray[i], reverse_val);
	}
	
	pPCGCall->GenSDFMapDispatch(m_pContext, nodeData.TexSize);

	//reverse_val = !reverse_val;
}

void Diligent::PCGTerrainTile::ReadBackPositionDataToHost()
{		
	m_pContext->CopyBuffer(mPlantTypeNumBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
		mPlantTypeNumStageData, 0, F_LAYER_NUM * sizeof(uint32_t),
		RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	m_pContext->CopyBuffer(mPlantPositionBuffers, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
		mPlantPosStageDatas, 0, F_LAYER_NUM * sizeof(float4) * PCG_PLANT_MAX_POSITION_NUM,
		RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	//sync gpu finish copy operation.
	m_pContext->WaitForIdle();

	MapHelper<uint32_t> map_plant_type_num_data(m_pContext, mPlantTypeNumStageData, MAP_READ, MAP_FLAG_DO_NOT_WAIT);
	MapHelper<float4> map_plant_position_data(m_pContext, mPlantPosStageDatas, MAP_READ, MAP_FLAG_DO_NOT_WAIT);
	
	uint32_t *pPlantTypeNum = new uint32_t[F_LAYER_NUM];
	memcpy(pPlantTypeNum, map_plant_type_num_data.GetMapData(), sizeof(uint32_t) * F_LAYER_NUM);
	mPlantTypeNumHostData.reset(pPlantTypeNum);

	mPlantPositionHostDatas.resize(F_LAYER_NUM);
	for (int i = 0; i < F_LAYER_NUM; ++i)
	{
		uint32_t plant_type_num = mPlantTypeNumHostData[i];

		float4 *pPlantPosDatas = new float4[plant_type_num];

		float4 *pSrcData = reinterpret_cast<float4*>(map_plant_position_data.GetMapData() + i * PCG_PLANT_MAX_POSITION_NUM);
		memcpy(pPlantPosDatas, pSrcData, sizeof(float4) * plant_type_num);

		mPlantPositionHostDatas[i].reset(pPlantPosDatas);
	}
}

Diligent::PCGResultData Diligent::PCGTerrainTile::GetPCGResultData()
{
	return PCGResultData(mPlantPositionHostDatas, mPlantTypeNumHostData);
}
