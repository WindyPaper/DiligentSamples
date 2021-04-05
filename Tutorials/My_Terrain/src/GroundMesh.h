#ifndef _GROUND_MESH_H_
#define _GROUND_MESH_H_

#pragma once

#include <memory>
#include <vector>
#include "Buffer.h"
#include "BasicMath.hpp"
#include "DeviceContext.h"
#include "RefCntAutoPtr.hpp"
#include "RenderDevice.h"

#include "CDLODTree.h"

namespace Diligent
{
	class FirstPersonCamera;

	struct ClipMapTerrainVerticesData
	{
		float2 XZ;
	};

	struct Patch
	{
		uint Offset;
		uint Count;
		uint2 Size;
	};

	struct PerPatchShaderData
	{
		float4 Scale;
		float4 Offset;		
	};

	class GroundMesh
	{
	public:
		GroundMesh(const uint SizeM, const uint Level, const float ClipScale);
		~GroundMesh();

		void InitClipMap(IRenderDevice *pDevice, ISwapChain *pSwapChain);		
		void Render(IDeviceContext *pContext);

		void Update(const FirstPersonCamera *pCam);		

	protected:
		void InitVertexBuffer();
		void InitIndicesBuffer();
		void CommitToGPUDeviceBuffer(IRenderDevice *pDevice);

		void UpdateLevelOffset(const float2& CamPosXZ);

		void InitPSO(IRenderDevice *pDevice, ISwapChain *pSwapChain, const Dimension& dim);

		uint16_t* GeneratePatchIndices(uint16_t *pIndexdata, uint VertexOffset, uint SizeX, uint SizeZ);

		int PatchIndexCount(const uint sizex, const uint sizez) const;

		float2 GetOffsetLevel(const float2 &CamPosXZ, const uint Level) const;

	private:
		uint m_sizem;
		uint m_level;
		float m_clip_scale;

		ClipMapTerrainVerticesData* m_pVerticesData;
		uint16_t *m_pIndicesData;
		uint m_VertexNum;
		uint m_IndexNum;

		RefCntAutoPtr<IBuffer> m_pVertexGPUBuffer;
		RefCntAutoPtr<IBuffer> m_pIndexGPUBuffer;
		RefCntAutoPtr<IBuffer> m_pVsConstBuf;
		RefCntAutoPtr<IBuffer> m_pVSTerrainInfoBuf;
		RefCntAutoPtr<IBuffer> m_pVsPatchBuf;
		RefCntAutoPtr<IShaderResourceBinding> m_pSRB;

		RefCntAutoPtr<IPipelineState> m_pPSO;

		std::vector<float2> m_LevelOffsets;

		Patch m_block;

		float4x4 m_TerrainViewProjMat;

		PerPatchShaderData m_PatchInstanceConst[16];//test for lv 0

		TerrainHeightMap m_Heightmap;

		//------------------CDLOD
		CDLODTree *mpCDLODTree;
	};
}

#endif