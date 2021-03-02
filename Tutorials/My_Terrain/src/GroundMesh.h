#ifndef _GROUND_MESH_H_
#define _GROUND_MESH_H_

#pragma once

#include <memory>
#include "Buffer.h"
#include "BasicMath.hpp"
#include "DeviceContext.h"
#include "RefCntAutoPtr.hpp"
#include "RenderDevice.h"

namespace Diligent
{
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

	class GroundMesh
	{
	public:
		GroundMesh(const uint SizeM, const uint Level, const float ClipScale);
		~GroundMesh();

		void InitClipMap(IRenderDevice *pDevice);
		void UpdateLevelOffset(const float2 CamPosXZ);
		void Render(IDeviceContext *pContext);

	protected:
		void InitVertexBuffer();
		void InitIndicesBuffer();
		void CommitToGPUDeviceBuffer(IRenderDevice *pDevice);

		void InitPSO(IRenderDevice *pDevice, ISwapChain *pSwapChain);

		uint16_t* GeneratePatchIndices(uint16_t *pIndexdata, uint VertexOffset, uint SizeX, uint SizeZ);

		int PatchIndexCount(const uint sizex, const uint sizez);

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
		RefCntAutoPtr<IShaderResourceBinding> m_pSRB;

		RefCntAutoPtr<IPipelineState> m_pPSO;

		Patch m_block;
	};
}

#endif