#pragma once

#include "BasicMath.hpp"
#include <string>
#include <vector>

struct HairVertexData
{
    Diligent::float3 Pos;
    int Misc;
};

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

struct HairData
{
    HairData();
    
    std::vector<int> HairIdxDataArray;
    std::vector<HairVertexData> HairVertexDataArray;

    Diligent::float3 HairBBoxMin;
    Diligent::float3 HairBBoxMax;
};
