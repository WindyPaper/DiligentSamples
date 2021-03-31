#ifndef _DEBUG_CANVAS_H_
#define _DEBUG_CANVAS_H_

#pragma once

#include <vector>
#include "AdvancedMath.hpp"
#include "Buffer.h"

namespace Diligent
{
	class IRenderDevice;
	class ISwapChain;
	class IDeviceContext;

	class DebugCanvas
	{
	public:
		DebugCanvas();
		~DebugCanvas();

		void AddDebugBox(const BoundBox &aabb);

		void Draw(IRenderDevice *pDevice, ISwapChain *pSwapChain, IDeviceContext *pContext);

	protected:
		void CreateVertexBuffer();
		void CreateIndexBuffer();

	private:
		bool mbInitGPUBuffer;
		std::vector<BoundBox> mAABBs;

		RefCntAutoPtr<IBuffer> mCubeVertexBuffer;
		RefCntAutoPtr<IBuffer> mCubeIndexBuffer;
	};

}

#endif