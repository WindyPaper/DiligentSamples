/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#pragma once

#include "SampleBase.hpp"
#include "FirstPersonCamera.hpp"

namespace Diligent
{
	class BVH;
	class BVHTrace;
	class BVHMeshData;

	struct RasterMeshData
	{
		RefCntAutoPtr<IBuffer> apMeshVertexData;
		RefCntAutoPtr<IBuffer> apMeshIndexData;
		BVHMeshData *pBVHMeshData;
		std::string meshName;
		float4 bbox_min;
		float4 bbox_max;

		RasterMeshData() :
			pBVHMeshData(nullptr)
		{
			float maxi_float = std::numeric_limits<float>::max();
			bbox_min = float4(maxi_float, maxi_float, maxi_float, maxi_float);

			float mini_float = std::numeric_limits<float>::min();
			bbox_max = float4(mini_float, mini_float, mini_float, mini_float);
		}
	};

class MyRayTracing final : public SampleBase
{
public:
	virtual ~MyRayTracing();

    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "My Ray tracing"; }

	virtual void WindowResize(Uint32 Width, Uint32 Height);

protected:
	RasterMeshData load_mesh(const std::string MeshName);

	void CreateNormalObjPSO();

	void UpdateUI();

private:
    RefCntAutoPtr<IPipelineState> m_pPSO;
	RefCntAutoPtr<IShaderResourceBinding> m_pSRB;
	RefCntAutoPtr<IShaderSourceInputStreamFactory> m_pShaderSourceFactory;

	RefCntAutoPtr<IPipelineState> m_pNormalObjPSO;
	RefCntAutoPtr<IShaderResourceBinding> m_pNormalObjSRB;

	RefCntAutoPtr<IBuffer> m_VSConstants;
	RefCntAutoPtr<ITexture> m_apNoiseTex;

	FirstPersonCamera m_Camera;

	float3 BakeInitDir;

	BVH *m_pMeshBVH;
	BVHTrace *m_pTrace;

	//RefCntAutoPtr<IBuffer> m_apPlaneMeshVertexData;
	//RefCntAutoPtr<IBuffer> m_apPlaneMeshIndexData;
	//BVHMeshData *m_pPlaneMeshData;

	float mTestPlaneOffsetY;
	float mBakeHeightScale;
	float mBakeTexTiling;

	std::vector<RasterMeshData> RasterMeshVec;
};

} // namespace Diligent
