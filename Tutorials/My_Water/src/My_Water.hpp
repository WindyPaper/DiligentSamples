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
#include "LightManager.h"

//RIGHT HAND COORDINATION

#define WATER_FFT_N 32

namespace Diligent
{
class GroundMesh;
struct TerrainVertexAttrData
{
	float3 pos;
	float4 color;

	TerrainVertexAttrData() :
		pos(0.0f, 0.0f, 0.0f),
		color(1.0f, 0.0f, 0.0f, 1.0f)
	{

	}
};

struct WaterFFTH0Uniform
{
	float4 N_L_Amplitude_Intensity; //pack
	float4 WindDir_LL_Alignment; //pack
};

struct WaterFFTHKTUniform
{
	float4 N_L_Time_Padding; //pack
};

struct WaterData
{
	enum WaterDataEnum
	{
		NOISE_TEX_NUM = 4
	};

	RefCntAutoPtr<ITexture> NoiseTextures[NOISE_TEX_NUM];

	void Init(IRenderDevice* pDevice);

protected:
	Uint8 BitReverseValue(const Uint8 FFTN);
};

struct TerrainData
{
	RefCntAutoPtr<IBuffer> pVertexBuf;
	RefCntAutoPtr<IBuffer> pIdxBuf;

	int VertexNum = 0;
	int IdxNum = 0;
};

class My_Water final : public SampleBase
{
public:
    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Tutorial01: Hello Triangle"; }

	virtual void WindowResize(Uint32 Width, Uint32 Height);

protected:
	void UpdateUI();
	void CreateGridBuffer();
	void CreateConstantsBuffer();
	void CreateComputePSO();

	void WaterRender();

private:
	void CreateH0PSO();
	void CreateHKTPSO();

private:
    RefCntAutoPtr<IPipelineState> m_pPSO;
	RefCntAutoPtr<IShaderSourceInputStreamFactory> m_pShaderSourceFactory;

	/*RefCntAutoPtr<IBuffer> m_pVertexBuf;
	RefCntAutoPtr<IBuffer> m_pIndexBuf;*/
	TerrainData m_TerrainData;

	RefCntAutoPtr<IBuffer> m_pVsConstBuf;
	RefCntAutoPtr<IShaderResourceBinding> m_pSRB;

	float4x4 m_TerrainWorldMatrix;

	FirstPersonCamera m_Camera;
	RefCntAutoPtr<IBuffer> m_CameraAttribsCB;
	MouseState        m_LastMouseState;

	std::shared_ptr<GroundMesh> m_apClipMap;

	LightManager m_LightManager;

	//Water
	WaterData mWaterData;

	//H0 & H0Minusk
	RefCntAutoPtr<IPipelineState> m_apH0PSO;
	RefCntAutoPtr<IShaderResourceBinding> m_apH0ResDataSRB;
	RefCntAutoPtr<IBuffer> m_apConstants;
	RefCntAutoPtr<IBuffer> m_apH0Buffer;
	RefCntAutoPtr<IBuffer> m_apH0MinuskBuffer;

	//Hkt
	RefCntAutoPtr<IPipelineState> m_apHKTPSO;
	RefCntAutoPtr<IShaderResourceBinding> m_apHKTDataSRB;
	RefCntAutoPtr<IBuffer> m_apHKTDX;
	RefCntAutoPtr<IBuffer> m_apHKTDY;
	RefCntAutoPtr<IBuffer> m_apHKTDZ;
	RefCntAutoPtr<IBuffer> m_apHKTConstData;

	int m_CSGroupSize;
};

} // namespace Diligent
