#include "HairData.h"

#include <limits>

HairData::HairData()
{
    const std::string idx_filename("./CompactLineIndices.bin");
    ReadFile(idx_filename, HairIdxDataArray);
    
    const std::string vertex_filename("./Vertices.bin");
    ReadFile(vertex_filename, HairVertexDataArray);

    //cal min/max bbox
    float max_float = std::numeric_limits<float>::max();
    HairBBoxMin = Diligent::float3(max_float, max_float, max_float);
    float min_float = std::numeric_limits<float>::min();
    HairBBoxMax = Diligent::float3(min_float, min_float, min_float);
    for(int i = 0; i < HairVertexDataArray.size(); ++i)
    {
        const HairVertexData &data = HairVertexDataArray[i];

        const Diligent::float3 &pos = data.Pos;

        if(!std::isnan(pos.x))
        {
            HairBBoxMin = Diligent::min(HairBBoxMin, pos);
            HairBBoxMax = Diligent::max(HairBBoxMax, pos);
        }
    }
}
