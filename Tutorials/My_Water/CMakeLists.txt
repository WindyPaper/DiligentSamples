cmake_minimum_required (VERSION 3.6)

project(My_Water CXX)

set(SOURCE	
	src/EpipolarLightScattering.cpp	
	src/LightManager.cpp
	src/OceanWave.cpp
	src/ReflectionProbe.cpp
	src/RenderProfile.cpp
	src/ShaderUniformDataMgr.cpp
    src/My_Water.cpp
    src/GroundMesh.cpp
    src/CDLODTree.cpp
    src/DebugCanvas.cpp
    src/TerrainMap.cpp
)

set(INCLUDE
	src/ImGuiProfiler/ImGuiProfilerRenderer.h
	src/ImGuiProfiler/ProfilerTask.h
	src/EpipolarLightScattering.hpp	
	src/LightManager.h
	src/OceanWave.h
	src/ReflectionProbe.h
	src/RenderProfile.h
	src/ShaderUniformDataMgr.h
    src/My_Water.hpp
    src/GroundMesh.h
    src/CDLODTree.h
    src/DebugCanvas.h
    src/TerrainMap.h
)

set(SHADERS)
set(ASSETS)

add_sample_app("My_Water" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
