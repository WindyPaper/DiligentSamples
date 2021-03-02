cmake_minimum_required (VERSION 3.6)

project(My_Terrain CXX)

set(SOURCE
    src/My_Terrain.cpp
    src/GroundMesh.cpp
)

set(INCLUDE
    src/My_Terrain.hpp
    src/GroundMesh.h
)

set(SHADERS)
set(ASSETS)

add_sample_app("My_Terrain" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
