#include "RenderProfile.h"


using hrc = std::chrono::high_resolution_clock;

Diligent::CPUProfileScope::CPUProfileScope(RenderProfileMgr *pRenderProfileMgr, std::string name, uint32_t TextColor /*= Colors::orange*/)
{
	m_pProfilerTask = pRenderProfileMgr->GetCPUProfilerTask();

	m_pProfilerTask->startTime = GetCurrFrameTimeSeconds();
	m_pProfilerTask->color = TextColor;
	m_pProfilerTask->name = name;
}

Diligent::CPUProfileScope::~CPUProfileScope()
{
	m_pProfilerTask->endTime = GetCurrFrameTimeSeconds();
}

double Diligent::CPUProfileScope::GetCurrFrameTimeSeconds()
{	
	return double(std::chrono::duration_cast<std::chrono::microseconds>(hrc::now() - m_StartTime).count()) / 1e6;
}

Diligent::GPUProfileScope::GPUProfileScope(RenderProfileMgr *pRenderProfileMgr, std::string name, uint32_t TextColor /*= Colors::orange*/)
{
	m_pRenderProfileMgr = pRenderProfileMgr;

	m_pProfilerTask = pRenderProfileMgr->GetGPUProfilerTask();
	m_pProfilerTask->name = name;
	m_pProfilerTask->color = TextColor;
	m_pProfilerTask->startTime = 0;

	pRenderProfileMgr->GPUProfileTaskStart();
}

Diligent::GPUProfileScope::~GPUProfileScope()
{
	double DurationTime;
	m_pRenderProfileMgr->GPUProfileTaskEnd(DurationTime);
	m_pProfilerTask->endTime = DurationTime;
}

void Diligent::RenderProfileMgr::Initialize(IRenderDevice *pDevice, IDeviceContext *pImmediateContext)
{
	CleanProfileTask();
	m_pImmediateContext = pImmediateContext;

	{
		QueryDesc queryDesc;
		queryDesc.Name = "Pipeline statistics query";
		queryDesc.Type = QUERY_TYPE_PIPELINE_STATISTICS;
		m_pPipelineStatsQuery.reset(new ScopedQueryHelper{ pDevice, queryDesc, 2 });
	}

	{
		QueryDesc queryDesc;
		queryDesc.Name = "Occlusion query";
		queryDesc.Type = QUERY_TYPE_OCCLUSION;
		m_pOcclusionQuery.reset(new ScopedQueryHelper{ pDevice, queryDesc, 2 });
	}

	{
		QueryDesc queryDesc;
		queryDesc.Name = "Duration query";
		queryDesc.Type = QUERY_TYPE_DURATION;
		m_pDurationQuery.reset(new ScopedQueryHelper{ pDevice, queryDesc, 2 });
	}

	m_pDurationFromTimestamps.reset(new DurationQueryHelper{ pDevice, 2 });
}

Diligent::RenderProfileMgr::RenderProfileMgr()
{

}

Diligent::RenderProfileMgr::~RenderProfileMgr()
{
	
}

Diligent::ProfilerTask * Diligent::RenderProfileMgr::GetCPUProfilerTask()
{
	return &m_CPUProfileTasks[m_CurrCPUProfileTaskIndex++];
}

Diligent::ProfilerTask * Diligent::RenderProfileMgr::GetGPUProfilerTask()
{
	return &m_GPUProfileTasks[m_CurrGPUProfileTaskIndex++];
}

void Diligent::RenderProfileMgr::GPUProfileTaskStart()
{
	m_pDurationFromTimestamps->Begin(m_pImmediateContext);
}

void Diligent::RenderProfileMgr::GPUProfileTaskEnd(double &RefEndTime)
{
	m_pDurationFromTimestamps->End(m_pImmediateContext, RefEndTime);
}

void Diligent::RenderProfileMgr::CleanProfileTask()
{
	m_CurrCPUProfileTaskIndex = 0;
	m_CurrGPUProfileTaskIndex = 0;
}
