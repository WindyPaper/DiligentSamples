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

	struct GenAORayData
	{
		float3 dir;
	};

	struct GenVertexAORaysUniformData
	{
		int num_vertex;
	};

	struct GenAOColorData
	{
		float lum;
	};

	static const Uint32 VERTEX_AO_RAY_SAMPLE_NUM = 256;

	class BVHTrace
	{
	public:
		BVHTrace(IDeviceContext *pDeviceCtx, IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory, ISwapChain* pSwapChain, BVH *pBVH, const FirstPersonCamera &cam);
		~BVHTrace();

		void Update(const FirstPersonCamera &cam);

		void DispatchBVHTrace();

		ITexture *GetOutputPixelTex();

		void DispatchVertexAO();

	protected:
		RefCntAutoPtr<IShader> CreateShader(const std::string &entryPoint, const std::string &csFile, const std::string &descName, const SHADER_TYPE type = SHADER_TYPE_COMPUTE, ShaderMacroHelper *pMacro = nullptr);
		PipelineStateDesc CreatePSODescAndParam(ShaderResourceVariableDesc *params, const int varNum, const std::string &psoName, const PIPELINE_TYPE type = PIPELINE_TYPE_COMPUTE);

		void CreateGenVertexAORaysPSO();
		void CreateGenVertexAORaysBuffer();
		void GenVertexAORays();

		void CreateVertexAOTracePSO();
		void CreateVertexAOTraceBuffer();

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

		//vertex ao gen rays
		RefCntAutoPtr<IPipelineState> m_apGenVertexAORaysPSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apGenVertexAORaysSRB;
		RefCntAutoPtr<IBuffer> m_apVertexAORaysUniformBuffer;
		RefCntAutoPtr<IBuffer> m_apVertexAOOutRaysBuffer;
		//vertex ao trace
		RefCntAutoPtr<IPipelineState> m_apVertexAOTracePSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apVertexAOTraceSRB;
		RefCntAutoPtr<IBuffer> m_apVertexAOColorBuffer;
		
		FirstPersonCamera m_Camera;
	};
}

#endif