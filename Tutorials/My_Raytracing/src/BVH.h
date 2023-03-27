#pragma once

#ifndef _BVH_H_
#define _BVH_H_

#include <limits>

#include "BasicMath.hpp"
#include "RefCntAutoPtr.hpp"
#include "Shader.h"
#include "Buffer.h"
#include "PipelineState.h"
#include "ShaderResourceBinding.h"
#include "ShaderMacroHelper.hpp"

namespace Diligent
{
	struct IRenderDevice;
	struct IDeviceContext;

	struct BVHVertex
	{
		float3 pos;
		float2 uv;
	};

	struct BVHMeshData
	{
		Uint32 vertex_num;
		Uint32 index_num;
		Uint32 primitive_num;
		Uint32 upper_pow_of_2_primitive_num;
	};

	struct BVHAABB
	{
		float4 upper;
		float4 lower;

		BVHAABB() :
			upper(std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), 0.0f),
			lower(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 0.0f)
		{}
	};

	struct BVHGlobalData
	{
		int num_objects;
		int num_interal_nodes;
		int num_all_nodes;
		int upper_pow_of_2_primitive_num;
	};

	struct SortMortonUniformData
	{
		Uint32 pass_id;
		Uint32 sort_num;
	};

	struct ReductionUniformData
	{
		Uint32 InReductionDataNum;
		Uint32 InAABBIdxOffset;
	};

	struct BVHNode
	{
		Uint32 parent_idx;
		Uint32 left_idx;
		Uint32 right_idx;
		Uint32 object_idx;

		BVHNode() :
			parent_idx(0xFFFFFFFF),
			left_idx(0xFFFFFFFF),
			right_idx(0xFFFFFFFF),
			object_idx(0xFFFFFFFF)
		{}
	};

	static const Uint32 ReductionGroupThreadNum = 512;
	static const Uint32 SortMortonCodeThreadNum = 1024;

	class BVH
	{
	public:
		BVH(IDeviceContext *pDeviceCtx, IRenderDevice *pDevice, IShaderSourceInputStreamFactory *pShaderFactory);
		~BVH();

		void InitTestMesh();

		void InitBuffer();

		void InitPSO();

		void BuildBVH();

	protected:
		RefCntAutoPtr<IShader> CreateShader(const std::string &entryPoint, const std::string &csFile, const std::string &descName, const SHADER_TYPE type = SHADER_TYPE_COMPUTE, ShaderMacroHelper *pMacro = nullptr);
		PipelineStateDesc CreatePSODescAndParam(ShaderResourceVariableDesc *params, const int varNum, const std::string &psoName, const PIPELINE_TYPE type = PIPELINE_TYPE_COMPUTE);

		void CreatePrimCenterMortonCodeData(int num);
		void CreatePrimAABBData(int num);
		void CreateGlobalBVHData();
		void CreateSortMortonCodeData(int num);

		void CreateConstructBVHData(int num_node);
		void CreateGenerateInternalAABBData(int num_internal_node);
		
		void CreateGenerateAABBPSO();
		void CreateReductionWholeAABBPSO();
		void CreateGenerateMortonCodePSO();
		void CreateSortMortonCodePSO();

		void DispatchAABBBuild();
		void ReductionWholeAABB();

		void DispatchMortonCodeBuild();
		void DispatchSortMortonCode();		

		//bvh start
		void CreateConstructBVHPSO();
		void _CreateInitBVHNodePSO();

		void DispatchInitBVHNode();
		
		void _CreateContructInternalNodePSO();
		void DispatchConstructBVHInternalNode();

		void _CreateGenerateInternalNodeAABBPSO();
		void DispatchGenerateInternalNodeAABB();

	private:
		IDeviceContext *m_pDeviceCtx;
		IRenderDevice *m_pDevice;
		IShaderSourceInputStreamFactory *m_pShaderFactory;

		RefCntAutoPtr<IBuffer> m_apMeshVertexData;
		RefCntAutoPtr<IBuffer> m_apMeshIndexData;		

		BVHMeshData m_BVHMeshData;

		RefCntAutoPtr<IBuffer> m_apGlobalBVHData;

		//gen AABB
		RefCntAutoPtr<IPipelineState>         m_apGenAABBPSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apGenAABBSRB;
		RefCntAutoPtr<IBuffer> m_apPrimAABBData;
		RefCntAutoPtr<IBuffer> m_apPrimCentroidData;

		RefCntAutoPtr<IPipelineState>         m_apWholeAABBPSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apWholeAABBSRB;
		RefCntAutoPtr<IBuffer> m_apWholeAABBDataPing;
		RefCntAutoPtr<IBuffer> m_apWholeAABBDataPong;
		RefCntAutoPtr<IBuffer> m_apReductionUniformData;
		IBuffer* m_pOutWholeAABB;

		//gen morton code
		RefCntAutoPtr<IPipelineState>         m_apGenMortonCodePSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apGenMortonCodeSRB;
		RefCntAutoPtr<IBuffer> m_apPrimCenterMortonCodeData;

		//simple sort morton code
		RefCntAutoPtr<IPipelineState> m_apSortMortonCodePSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apSortMortonCodeSRB;
		RefCntAutoPtr<IBuffer> m_apOutSortMortonCodeData;
		RefCntAutoPtr<IBuffer> m_apOutSortIdxDataPing;
		RefCntAutoPtr<IBuffer> m_apOutSortIdxDataPong;
		IBuffer* m_pOutResultSortData;
		IBuffer* m_pOutResultIdxData;
		RefCntAutoPtr<IBuffer> m_apSortMortonCodeUniform;

		//bvh
		RefCntAutoPtr<IPipelineState> m_apInitBVHNodePSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apInitBVHNodeSRB;
		RefCntAutoPtr<IBuffer> m_apBVHNodeData;

		RefCntAutoPtr<IPipelineState> m_apConstructInternalNodePSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apConstructInternalNodeSRB;

		RefCntAutoPtr<IPipelineState> m_apGenerateInternalNodeAABBPSO;
		RefCntAutoPtr<IShaderResourceBinding> m_apGenerateInternalNodeAABBSRB;
		RefCntAutoPtr<IBuffer> m_apGenerateInternalNodeFlagData;
		//RefCntAutoPtr<IBuffer> m_ap
	};
}

#endif
