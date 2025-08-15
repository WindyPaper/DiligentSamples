#include "HairData.h"

#include <limits>

template<class T>
int ReadFile(std::string filename, std::vector<T> &Array)
{
    //string filename("./CompactLineIndices.bin");
    FILE* file;
    fopen_s(&file, filename.c_str(), "rb");
    if (!file) {
        throw std::runtime_error("Could not open file");
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // 分配内存并读取
    //std::vector<char> buffer(size);
    //*pReadBuf = new char[size];
    Array.resize(size / sizeof(T));
    size_t bytesRead = fread((char*)&Array[0], 1, size, file);

    fclose(file);

    if (bytesRead != static_cast<size_t>(size)) {
        throw std::runtime_error("Could not read entire file");
    }

    return size;
}

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
