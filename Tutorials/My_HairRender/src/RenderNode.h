#pragma once

#include <string>

#include "RefCntAutoPtr.hpp"
#include "RenderDevice.h"
#include "Shader.h"
#include "ShaderMacroHelper.hpp"

namespace Diligent
{
struct IDeviceContext;
struct ISwapChain;
struct IShader;

struct RenderPassNode
{		
    RenderPassNode(Diligent::IDeviceContext *pDC);
    RenderPassNode &Clear(Diligent::ISwapChain *pSwapChain);


    Diligent::IDeviceContext *pDeviceContext;
};

class IBaseRender
{
public:
    explicit IBaseRender(IDeviceContext *pDeviceCtx, \
        IRenderDevice *pDevice, \
        IShaderSourceInputStreamFactory *pShaderFactory);

    RefCntAutoPtr<IBuffer> CreateConstBuffer(const int size, void* pInitialData = nullptr, const std::string &desc = "");
    RefCntAutoPtr<IBuffer> CreateStructureBuffer(const int element_size, const int count, void* pInitialData = nullptr, const std::string &desc = "");
    
    RefCntAutoPtr<IShader> CreateShader(const std::string &entryPoint, const std::string &csFile, const std::string &descName, const SHADER_TYPE type = SHADER_TYPE_COMPUTE, ShaderMacroHelper *pMacro = nullptr);
    PipelineStateDesc CreatePSODescAndParam(ShaderResourceVariableDesc *params, const int varNum, const std::string &psoName, const PIPELINE_TYPE type = PIPELINE_TYPE_COMPUTE);

protected:
    IDeviceContext *m_pDeviceCtx;
    IRenderDevice *m_pDevice;
    IShaderSourceInputStreamFactory *m_pShaderFactory;
};
}
