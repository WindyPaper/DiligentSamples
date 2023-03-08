cmake_minimum_required (VERSION 3.6)

project(My_PCGFoliage CXX)

set(SOURCE
    src/My_Terrain.cpp
    src/GroundMesh.cpp
    src/CDLODTree.cpp
    src/DebugCanvas.cpp
    src/TerrainMap.cpp

    src/CDLODTree.cpp
    src/PCGCSCall.cpp
    src/PCGLayer.cpp
    src/PCGNodePool.cpp
    src/PCGPoint.cpp
    src/PCGSystem.cpp
    src/ProxyCube.cpp
)

set(INCLUDE
    src/My_Terrain.hpp
    src/GroundMesh.h
    src/CDLODTree.h
    src/DebugCanvas.h
    src/TerrainMap.h

    src/CDLODTree.h
    src/MortonCode.h
    src/PCGCSCall.h
    src/PCGLayer.h
    src/PCGNodePool.h
    src/PCGPoint.h
    src/PCGSystem.h
    src/ProxyCube.h
)

set(SHADERS)
set(ASSETS)

add_sample_app("My_PCGFoliage" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
