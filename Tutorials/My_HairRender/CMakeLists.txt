cmake_minimum_required (VERSION 3.6)

project(My_HairRender CXX)

set(SOURCE
    src/Tutorial02_Cube.cpp
)

set(INCLUDE
    src/Tutorial02_Cube.hpp
)

set(SHADERS
    assets/cube.vsh
    assets/cube.psh
)

set(ASSETS)

add_sample_app("My_HairRender" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
