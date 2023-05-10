#pragma once

#ifndef _BVH_TRACE_H_
#define _BVH_TRACE_H_

#include "FirstPersonCamera.hpp"
#include "BasicMath.hpp"
#include "RefCntAutoPtr.hpp"
#include "Shader.h"
#include "Buffer.h"
#include "Texture.h"
#include "PipelineState.h"
#include "ShaderResourceBinding.h"
#include "ShaderMacroHelper.hpp"

namespace Diligent
{
	struct IRenderDevice;
	struct IDeviceContext;
	struct ISwapChain;
	class BVH;

	struct TraceUniformData
	{
		float4x4 InvViewProjMatrix;
		float4 CameraWPos;
		float2 ScreenSize;
		float2 Padding;
	};

	class BVHTrace
	{
	public:
		BVHTrace(IDeviceContext *pDeviceCtx, IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory, ISwapChain* pSwapChain, BVH *pBVH, const FirstPersonCamera &cam);
		~BVHTrace();

		void Update(const FirstPersonCamera &cam);

		void DispatchBVHTrace();

		ITexture *GetOutputPixelTex();

	protected:
		RefCntAutoPtr<IShader> CreateShader(const std::string &entryPoint, const std::string &csFile, const std::string &descName, const SHADER_TYPE type = SHADER_TYPE_COMPUTE, ShaderMacroHelper *pMacro = nullptr);
		PipelineStateDesc CreatePSODescAndParam(ShaderResourceVariableDesc *params, const int varNum, const std::string &psoName, const PIPELINE_TYPE type = PIPELINE_TYPE_COMPUTE);

		void CreateGenVertexAORaysPSO();
		void GenVertexAORays();

		void CreateTracePSO();
		void CreateBuffer();
		void BindDiffTexs();

	private:
		BVH *m_pBVH;

		IDeviceContext *m_pDeviceCtx;
		IRenderDevice *m_pDevice;
		IShaderSourceInputStreamFactory *m_pShaderFactory;
		ISwapChain* m_pSwapChain;

		RefCntAutoPtr<IPipelineState> m_apTracePSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apTraceSRB;
		RefCntAutoPtr<IBuffer> m_apTraceUniformData;
		RefCntAutoPtr<ITexture> m_apOutRTPixelTex;

		//vertex ao
		RefCntAutoPtr<IPipelineState> m_apGenVertexAORaysPSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apGenVertexAORaysSRB;
		RefCntAutoPtr<IBuffer> m_apVertexAORaysBuffer;
		
		FirstPersonCamera m_Camera;
	};
}

#endif