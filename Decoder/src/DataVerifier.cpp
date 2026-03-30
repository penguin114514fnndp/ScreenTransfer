#include "DataVerifier.h"
#include <iostream>
#include <fstream>
#include <iomanip>

bool DataVerifier::LoadFile(const fs::path& path, std::vector<uint8_t>& data)
{
    try
    {
        if (!fs::exists(path))
            return false;

        std::ifstream f(path, std::ios::binary);
        if (!f)
            return false;

        f.seekg(0, std::ios::end);
        auto size = f.tellg();
        f.seekg(0);

        data.resize(size);
        f.read((char*)data.data(), size);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void DataVerifier::Compare(const fs::path& file1, const fs::path& file2)
{
    std::vector<uint8_t> data1, data2;

    // 读取两个文件
    if (!LoadFile(file1, data1))
    {
        std::cerr << "Error: Cannot load " << file1 << "\n";
        return;
    }

    if (!LoadFile(file2, data2))
    {
        std::cerr << "Error: Cannot load " << file2 << "\n";
        return;
    }

    std::cout << "\n=== DATA COMPARISON ===\n";
    std::cout << file1.filename() << ": " << data1.size() << " bytes\n";
    std::cout << file2.filename() << ": " << data2.size() << " bytes\n";

    // 比对大小
    if (data1.size() != data2.size())
    {
        std::cout << "\nResult: SIZE MISMATCH\n";
        std::cout << "=======================\n\n";
        return;
    }

    // 完全匹配
    if (data1 == data2)
    {
        std::cout << "\nResult: PERFECT MATCH (100% integrity)\n";
        std::cout << "=======================\n\n";
        return;
    }

    // 统计差异字节数
    int diffCount = 0;
    int firstDiff = -1;
    for (size_t i = 0; i < data1.size(); i++)
    {
        if (data1[i] != data2[i])
        {
            if (firstDiff < 0)
                firstDiff = i;
            diffCount++;
        }
    }

    double rate = 100.0 * (data1.size() - diffCount) / data1.size();
    std::cout << "\nResult: " << diffCount << " bytes differ ("
              << std::fixed << std::setprecision(2) << rate << "% match)\n";
    if (firstDiff >= 0)
    {
        std::cout << "First diff at byte " << firstDiff << ": 0x"
                  << std::hex << (int)data1[firstDiff]
                  << " != 0x" << (int)data2[firstDiff] << std::dec << "\n";
    }
    std::cout << "=======================\n\n";
}
