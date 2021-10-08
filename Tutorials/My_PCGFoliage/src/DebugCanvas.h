#ifndef _DEBUG_CANVAS_H_
#define _DEBUG_CANVAS_H_

#pragma once

#include <vector>
#include "RefCntAutoPtr.hpp"
#include "AdvancedMath.hpp"
#include "Buffer.h"
#include "PipelineState.h"

namespace Diligent
{
	struct IRenderDevice;
	class ISwapChain;
	class IDeviceContext;
	class FirstPersonCamera;
	struct IShaderSourceInputStreamFactory;

	struct AABBInstanceData
	{
		float4 scale;
		float4 offset;
	};

	class DebugCanvas
	{
	public:
		enum
		{
			MAX_AABB_COUNT = 4086
		};

		DebugCanvas();
		~DebugCanvas();

		void AddDebugBox(const BoundBox &aabb);
		void ClearAABB() { mAABBs.clear(); }

		void Draw(IRenderDevice* pDevice, ISwapChain* pSwapChain, IDeviceContext* pContext, IShaderSourceInputStreamFactory* pShaderFactory, const FirstPersonCamera* cam);

	protected:
		void CreateVertexBuffer(IRenderDevice* pDevice);
		void CreateIndexBuffer(IRenderDevice* pDevice);
		void CreateInstance(IRenderDevice* pDevice);
		void CreatePipelineState(IRenderDevice* pDevice, TEXTURE_FORMAT RTVFormat, TEXTURE_FORMAT DSVFormat, IShaderSourceInputStreamFactory* pShaderSourceFactory);
		void UpdateInstanceBuffer(IDeviceContext* pContext);

	private:
		bool mbInitGPUBuffer;
		std::vector<BoundBox> mAABBs;

		RefCntAutoPtr<IBuffer> mCubeVertexBuffer;
		RefCntAutoPtr<IBuffer> mCubeIndexBuffer;
		RefCntAutoPtr<IBuffer> mInstanceBuffer;
		RefCntAutoPtr<IBuffer> mVSConstants;

		RefCntAutoPtr<IPipelineState> mPSO;
		RefCntAutoPtr<IShaderResourceBinding> mSRB;
	};

}

#endif