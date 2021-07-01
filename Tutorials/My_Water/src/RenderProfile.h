#ifndef _RENDER_PROFILE_H_
#define _RENDER_PROFILE_H_

#pragma once

#include <chrono>
#include <vector>
#include <memory>
#include "ImGuiProfiler/ProfilerTask.h"
#include "GraphicsTypes.h"
#include "ScopedQueryHelper.hpp"
#include "DurationQueryHelper.hpp"

namespace Diligent
{
	class IDeviceContext;
	class IRenderDevice;

	class RenderProfileMgr;

	class CPUProfileScope
	{
	public:
		CPUProfileScope(RenderProfileMgr *pRenderProfileMgr, std::string name, uint32_t TextColor = Colors::orange);
		~CPUProfileScope();
	
	protected:
		double GetCurrFrameTimeSeconds();		

	private:
		std::chrono::high_resolution_clock::time_point m_StartTime;
		ProfilerTask *m_pProfilerTask;
	};

	class GPUProfileScope
	{
	public:
		GPUProfileScope(RenderProfileMgr *pRenderProfileMgr, std::string name, uint32_t TextColor = Colors::orange);
		~GPUProfileScope();

	private:
		ProfilerTask *m_pProfilerTask;
		RenderProfileMgr *m_pRenderProfileMgr;
	};

	class RenderProfileMgr
	{
	public:
		RenderProfileMgr();
		void Initialize(IRenderDevice *pDevice, IDeviceContext *pImmediateContext);
		~RenderProfileMgr();

		ProfilerTask *GetCPUProfilerTask();
		ProfilerTask *GetGPUProfilerTask();

		void GPUProfileTaskStart();
		void GPUProfileTaskEnd(double &RefEndTime);

		void CleanProfileTask();

	private:
		std::unique_ptr<ScopedQueryHelper>   m_pPipelineStatsQuery;
		std::unique_ptr<ScopedQueryHelper>   m_pOcclusionQuery;
		std::unique_ptr<ScopedQueryHelper>   m_pDurationQuery;
		std::unique_ptr<DurationQueryHelper> m_pDurationFromTimestamps;

		IDeviceContext *m_pImmediateContext;

		std::vector<ProfilerTask> m_CPUProfileTasks;
		std::vector<ProfilerTask> m_GPUProfileTasks;
		int m_CurrGPUProfileTaskIndex;
		int m_CurrCPUProfileTaskIndex;
	};
}

#endif

