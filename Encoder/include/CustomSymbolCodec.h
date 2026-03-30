#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

class CustomSymbolCodec
{
public:
    static constexpr uint32_t kFrameMagic = 0x31445543; // "CUD1"的ASCII码，用于帧数据的标识   
    static constexpr size_t kFrameHeaderSize = 28;      // 每帧数据头部的字节数
    static constexpr int kCodeModules = 121;            // 二维码模块数（不含安静区）
    static constexpr int kQuietZoneModules = 4;         // 安静区模块数
    static constexpr int kModuleScale = 10;             // 每个模块对应的像素数
    static constexpr size_t kFrameByteCapacity = 1710;  // 每帧总字节数（含头部和负载）

    static size_t FrameByteCapacity() { return kFrameByteCapacity; } // 每帧总字节数（含头部和负载）
    static size_t PayloadCapacityPerFrame() { return kFrameByteCapacity - kFrameHeaderSize; }; // 每帧可用的负载字节数
    static std::vector<std::vector<uint8_t>> CreateFrames(const std::vector<uint8_t>& data); // 将输入数据切分成多帧
    static void SaveFrameAsPpm(const std::vector<uint8_t>& bytes, const fs::path& filePath); // 将帧数据保存为PPM图像

private:
    static bool IsReservedModule(int x, int y); // 判断模块是否为保留区域
    static void DrawFinder(std::vector<uint8_t>& modules, int x0, int y0); // 绘制定位图形
    static uint32_t ComputeCrc32(const uint8_t* data, size_t size); // 计算CRC32校验码
};
