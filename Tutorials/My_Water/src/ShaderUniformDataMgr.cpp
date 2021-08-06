#include "ShaderUniformDataMgr.h"

namespace Diligent
{
	Diligent::ShaderUniformDataMgr::ShaderUniformDataMgr()
	{
	}

	Diligent::ShaderUniformDataMgr::~ShaderUniformDataMgr()
	{

	}

	void Diligent::ShaderUniformDataMgr::CreateGPUBuffer(IRenderDevice *pDevice)
	{
		BufferDesc LightConstBufferDesc;
		LightConstBufferDesc.Name = "Shader Uniform Light Const Data";
		LightConstBufferDesc.Usage = USAGE_DYNAMIC;
		LightConstBufferDesc.BindFlags = BIND_UNIFORM_BUFFER;
		LightConstBufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
		LightConstBufferDesc.uiSizeInBytes = sizeof(XLightStructure);
		pDevice->CreateBuffer(LightConstBufferDesc, nullptr, &m_apGPULightStructure);
	}

	IBuffer * Diligent::ShaderUniformDataMgr::GetLightStructure()
	{
		return m_apGPULightStructure;
	}

	const IBuffer * Diligent::ShaderUniformDataMgr::GetLightStructure() const
	{
		return m_apGPULightStructure;
	}
}

