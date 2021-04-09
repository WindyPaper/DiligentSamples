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

	struct GPUConstBuffer
	{
		float4x4 ViewProj;
		float4 MorphKInfo; //[0]:end/dis, [1] 1/dis
		float4 CameraPos;
	};

	class GroundMesh
	{
	public:
		GroundMesh(const uint SizeM, const uint Level, const float ClipScale);
		~GroundMesh();

		void InitClipMap(IRenderDevice *pDevice, ISwapChain *pSwapChain);		
		void Render(IDeviceContext *pContext, const float3 &CamPos);

		void Update(const FirstPersonCamera *pCam);		

	protected:
		void InitVertexBuffer();
		void InitIndicesBuffer();
		void CommitToGPUDeviceBuffer(IRenderDevice *pDevice);

		void InitPSO(IRenderDevice *pDevice, ISwapChain *pSwapChain, const Dimension& dim);

		DrawIndexedAttribs GetDrawIndex(const uint16_t start, const uint16_t num);

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

		TerrainMap m_Heightmap;

		//MESH
		uint32_t m_indexEndTL;
		uint32_t m_indexEndTR;
		uint32_t m_indexEndBL;
		uint32_t m_indexEndBR;

		//------------------CDLOD
		CDLODTree *mpCDLODTree;
	};
}

#endif