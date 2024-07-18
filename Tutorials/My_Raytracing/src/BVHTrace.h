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
		float4 dir;
	};

	struct GenSubdivisionPosInTriangle
	{
		float3 pos;
	};

	struct GenVertexAORaysUniformData
	{
		int num_vertex;
	};

	struct GenAOColorData
	{
		float lum;
	};

	struct GenTriangleFaceAOUniformData
	{
		int num_triangle;
	};

	struct TraceBakeMeshData
	{
		float4 BakeVerticalNorDir;
	};

	static const Uint32 VERTEX_AO_RAY_SAMPLE_NUM = 256;
	static const Uint32 TRIANGLE_SUBDIVISION_NUM = 16;
	static const Uint32 BAKE_MESH_TEX_XY = 128;
	static const Uint32 BAKE_MESH_TEX_Z  = 32;

	class BVHTrace
	{
	public:
		BVHTrace(IDeviceContext *pDeviceCtx, IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory, ISwapChain* pSwapChain, BVH *pBVH, const FirstPersonCamera &cam, const std::string &mesh_file_name);
		~BVHTrace();

		void Update(const FirstPersonCamera &cam);

		void DispatchBVHTrace();

		ITexture *GetOutputPixelTex();

		ITexture *GetBakeMesh3DTexture();

		void DispatchVertexAOTrace();
		void DispatchTriangleAOTrace();

		void DispatchBakeMesh3DTexture(const float3 &BakeInitDir);

	protected:
		RefCntAutoPtr<IShader> CreateShader(const std::string &entryPoint, const std::string &csFile, const std::string &descName, const SHADER_TYPE type = SHADER_TYPE_COMPUTE, ShaderMacroHelper *pMacro = nullptr);
		PipelineStateDesc CreatePSODescAndParam(ShaderResourceVariableDesc *params, const int varNum, const std::string &psoName, const PIPELINE_TYPE type = PIPELINE_TYPE_COMPUTE);

		void CreateGenVertexAORaysPSO();
		void CreateGenVertexAORaysBuffer();
		void GenVertexAORays();

		void CreateGenTriangleAORaysPSO();
		void CreateGenTriangleAORaysBuffer();
		void CreateGenTriangleAOPosPSO();
		void CreateGenTriangleAOPosBuffer();
		void GenTriangleAORaysAndPos();

		void CreateVertexAOTracePSO();
		void CreateVertexAOTraceBuffer();

		void CreateTriangleAOTracePSO();
		void CreateTriangleAOTraceBuffer();

		void CreateTriangleFaceAOPSO();
		void CreateTriangleFaceAOBuffer();

		void CreateBakeMesh3DTexPSO();
		void CreateBakeMesh3DTexBuffer();

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
		RefCntAutoPtr<IBuffer> m_apVertexAOColorStageBuffer;		

		//triangle ao gen rays
		RefCntAutoPtr<IPipelineState> m_apGenTriangleAORaysPSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apGenTriangleAORaysSRB;
		//RefCntAutoPtr<IBuffer> m_apTriangleAORaysUniformBuffer;
		RefCntAutoPtr<IBuffer> m_apTriangleAOOutRaysBuffer;

		RefCntAutoPtr<IPipelineState> m_apGenTriangleAOPosPSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apGenTriangleAOPosSRB;
		RefCntAutoPtr<IBuffer> m_apTriangleAOOutPosBuffer;

		RefCntAutoPtr<IPipelineState> m_apGenTriangleAOTracePSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apGenTriangleAOTraceSRB;
		RefCntAutoPtr<IBuffer> m_apTriangleAOOutPosColorBuffer;

		RefCntAutoPtr<IPipelineState> m_apGenTriangleFaceAOPSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apGenTriangleFaceAOSRB;
		RefCntAutoPtr<IBuffer> m_apTriangleFaceAOColorUniformBuffer;
		RefCntAutoPtr<IBuffer> m_apTriangleFaceAOColorBuffer;
		RefCntAutoPtr<IBuffer> m_apTriangleFaceAOColorStageBuffer;

		RefCntAutoPtr<IPipelineState> m_apBakeMesh3DTexPSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apBakeMesh3DTexSRB;
		RefCntAutoPtr<ITexture> m_apBakeMesh3DTexData;
		RefCntAutoPtr<ITexture> m_apBakeMeshDiffTexData;
		RefCntAutoPtr<IBuffer> m_apBakeMesh3DTexUniformBuffer;


		FirstPersonCamera m_Camera;
		std::string m_mesh_file_name;
	};
}

#endif