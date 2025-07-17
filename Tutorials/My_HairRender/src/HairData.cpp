#include "HairData.h"

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

    // delete []pBuffer;
    // delete []pVertexBuf;
}
