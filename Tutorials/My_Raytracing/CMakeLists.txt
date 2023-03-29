cmake_minimum_required (VERSION 3.6)

project(My_Raytracing CXX)

set(SOURCE
    src/My_Raytracing.cpp
    src/BVH.cpp
    src/BVHTrace.cpp
)

set(INCLUDE
    src/My_Raytracing.hpp
    src/BVH.h
    src/BVHTrace.h
)

set(SHADERS)
set(ASSETS)

add_sample_app("My_Raytracing" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
