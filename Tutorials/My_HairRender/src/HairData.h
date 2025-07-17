#pragma once

#include "BasicMath.hpp"
#include <string>
#include <vector>

struct HairVertexData
{
    Diligent::float3 Pos;
    int Misc;
};

struct HairData
{
    HairData();
    
    std::vector<int> HairIdxDataArray;
    std::vector<HairVertexData> HairVertexDataArray;
};
