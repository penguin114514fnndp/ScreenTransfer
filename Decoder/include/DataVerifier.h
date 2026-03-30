#pragma once

#include <vector>
#include <cstdint>
#include <filesystem>

namespace fs = std::filesystem;

// 简单的文件比对工具
class DataVerifier
{
public:
    // 加载两个文件并比对，输出结果
    void Compare(const fs::path& file1, const fs::path& file2);

private:
    bool LoadFile(const fs::path& path, std::vector<uint8_t>& data);
};
