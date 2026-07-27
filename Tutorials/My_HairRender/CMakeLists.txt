cmake_minimum_required (VERSION 3.6)

project(My_HairRender CXX)

set(SOURCE
    src/Tutorial02_Cube.cpp
    src/HairRender.cpp
    src/HairData.cpp
    src/RenderNode.cpp
)

set(INCLUDE
    src/Tutorial02_Cube.hpp
    src/HairRender.h
    src/HairData.h
    src/RenderNode.h
)

set(SHADERS
    assets/cube.vsh
    assets/cube.psh
    assets/head.vsh
    assets/head.psh
)

set(ASSETS)

add_sample_app("My_HairRender" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
