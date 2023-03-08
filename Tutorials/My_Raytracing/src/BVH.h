#pragma once

#ifndef _BVH_H_
#define _BVH_H_

#include "BasicMath.hpp"
#include "RefCntAutoPtr.hpp"

namespace Diligent
{
	class IRenderDevice;
	class IShaderSourceInputStreamFactory;
	class IDeviceContext;

	struct Vertex
	{
		float3 pos;
		float2 uv;
	};

	class BVH
	{
	public:
		BVH(IDeviceContext *pDeviceCtx, IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);
		~BVH();

		void InitTestMesh();

	private:
		IDeviceContext *m_pDeviceCtx;
		IRenderDevice *m_pDevice;
		IShaderSourceInputStreamFactory *m_pShaderFactory;

		RefCntAutoPtr<IBuffer> m_apMeshVertexData;
		RefCntAutoPtr<IBuffer> m_apMeshIndexData;
	};
}

#endif
