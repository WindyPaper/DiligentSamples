cmake_minimum_required (VERSION 3.6)

project(My_Water CXX)

set(SOURCE
    src/My_Water.cpp
    src/GroundMesh.cpp
    src/CDLODTree.cpp
    src/DebugCanvas.cpp
    src/TerrainMap.cpp
)

set(INCLUDE
    src/My_Water.hpp
    src/GroundMesh.h
    src/CDLODTree.h
    src/DebugCanvas.h
    src/TerrainMap.h
)

set(SHADERS)
set(ASSETS)

add_sample_app("My_Water" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
