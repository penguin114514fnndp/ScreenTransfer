#include "DataVerifier.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>

namespace
{
int BitCount(uint8_t x)
{
    int count = 0;
    while (x != 0)
    {
        x &= static_cast<uint8_t>(x - 1);
        ++count;
    }
    return count;
}
}

bool DataVerifier::LoadFile(const fs::path& path, std::vector<uint8_t>& data)
{
    if (!fs::exists(path))
        return false;

    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;

    data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return static_cast<bool>(f) || f.eof();
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

    const size_t compareSize = std::max(data1.size(), data2.size());
    if (compareSize == 0)
    {
        std::cout << "\nResult: EMPTY DATA\n";
        std::cout << "=======================\n\n";
        return;
    }

    int matchedBytes = 0;
    int firstDiff = -1;
    uint64_t bitErrors = 0;
    for (size_t i = 0; i < compareSize; ++i)
    {
        const bool in1 = i < data1.size();
        const bool in2 = i < data2.size();
        const uint8_t b1 = in1 ? data1[i] : 0;
        const uint8_t b2 = in2 ? data2[i] : 0;

        if (in1 && in2 && b1 == b2)
        {
            ++matchedBytes;
            continue;
        }

        if (firstDiff < 0)
            firstDiff = static_cast<int>(i);

        if (!in1 || !in2)
            bitErrors += 8;
        else
            bitErrors += static_cast<uint64_t>(BitCount(static_cast<uint8_t>(b1 ^ b2)));
    }

    const double byteRate = 100.0 * matchedBytes / static_cast<double>(compareSize);
    const uint64_t totalBits = static_cast<uint64_t>(compareSize) * 8;
    const double bitRate = 100.0 * (totalBits - bitErrors) / static_cast<double>(totalBits);

    std::cout << "\nResult Summary:\n";
    if (data1.size() != data2.size())
        std::cout << "- Size mismatch: " << data1.size() << " vs " << data2.size() << " bytes\n";
    std::cout << "- Byte accuracy: " << std::fixed << std::setprecision(2)
              << byteRate << "% (" << matchedBytes << "/" << compareSize << ")\n";
    std::cout << "- Bit accuracy:  " << std::fixed << std::setprecision(2)
              << bitRate << "%\n";

    if (firstDiff >= 0)
    {
        std::cout << "- First diff at byte " << firstDiff << ": ";
        if (static_cast<size_t>(firstDiff) < data1.size())
            std::cout << "0x" << std::hex << static_cast<int>(data1[firstDiff]);
        else
            std::cout << "<missing>";

        std::cout << " vs ";
        if (static_cast<size_t>(firstDiff) < data2.size())
            std::cout << "0x" << std::hex << static_cast<int>(data2[firstDiff]);
        else
            std::cout << "<missing>";
        std::cout << std::dec << "\n";
    }

    if (byteRate == 100.0 && bitRate == 100.0 && data1.size() == data2.size())
    {
        std::cout << "- Integrity: PERFECT MATCH\n";
    }

    std::cout << "=======================\n\n";
}
