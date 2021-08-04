#include "ShaderUniformDataMgr.h"


Diligent::ShaderUniformDataMgr::ShaderUniformDataMgr()
{
	Init();
}

Diligent::ShaderUniformDataMgr::~ShaderUniformDataMgr()
{

}

void Diligent::ShaderUniformDataMgr::Init()
{
	BufferDesc LightConstBufferDesc;
	LightConstBufferDesc.Name = "Shader Uniform Light Const Data";
	LightConstBufferDesc.Usage = USAGE_DYNAMIC;
	LightConstBufferDesc.BindFlags = BIND_UNIFORM_BUFFER;
	LightConstBufferDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
	LightConstBufferDesc.uiSizeInBytes = sizeof(XLightStructure);
	m_apDevice->CreateBuffer(LightConstBufferDesc, nullptr, &m_apGPULightStructure);
}

