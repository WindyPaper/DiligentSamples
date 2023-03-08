#include "BVH.h"

#include "Buffer.h"
#include "Texture.h"
#include "PipelineState.h"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "Shader.h"

Diligent::BVH::BVH(IDeviceContext *pDeviceCtx, IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory) :
	m_pDeviceCtx(pDeviceCtx),
	m_pDevice(pDevice),
	m_pShaderFactory(pShaderFactory)
{

}

Diligent::BVH::~BVH()
{

}

void Diligent::BVH::InitTestMesh()
{
	Vertex CubeVerts[8] =
	{
		{float3(-1,-1,-1), float2(1,0)},
		{float3(-1,+1,-1), float2(0,1)},
		{float3(+1,+1,-1), float2(0,0)},
		{float3(+1,-1,-1), float2(1,1)},

		{float3(-1,-1,+1), float2(1,1)},
		{float3(-1,+1,+1), float2(0,1)},
		{float3(+1,+1,+1), float2(1,0)},
		{float3(+1,-1,+1), float2(1,1)},
	};

	//anti-clockwise
	Uint32 Indices[] =
	{
		2,0,1, 2,3,0,
		4,6,5, 4,7,6,
		0,7,4, 0,3,7,
		1,0,4, 1,4,5,
		1,5,2, 5,6,2,
		3,6,7, 3,2,6
	};

	// Create a vertex buffer that stores cube vertices
	BufferDesc VertBuffDesc;
	VertBuffDesc.Name = "Cube vertex buffer";
	VertBuffDesc.Usage = USAGE_IMMUTABLE;
	VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
	VertBuffDesc.uiSizeInBytes = sizeof(CubeVerts);
	BufferData VBData;
	VBData.pData = CubeVerts;
	VBData.DataSize = sizeof(CubeVerts);
	m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_apMeshVertexData);

	BufferDesc IndBuffDesc;
	IndBuffDesc.Name = "Cube index buffer";
	IndBuffDesc.Usage = USAGE_IMMUTABLE;
	IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
	IndBuffDesc.uiSizeInBytes = sizeof(Indices);
	BufferData IBData;
	IBData.pData = Indices;
	IBData.DataSize = sizeof(Indices);
	m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_apMeshIndexData);
}

