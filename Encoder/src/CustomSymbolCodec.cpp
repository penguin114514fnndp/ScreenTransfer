#include "CustomSymbolCodec.h"
#include <algorithm>
#include <fstream>

std::vector<std::vector<uint8_t>> CustomSymbolCodec::CreateFrames(const std::vector<uint8_t>& data)
{
    const size_t payloadSizePerFrame = PayloadCapacityPerFrame();
    std::vector<std::vector<uint8_t>> chunks;
    const uint32_t totalFrames = static_cast<uint32_t>((data.size() + payloadSizePerFrame - 1) / payloadSizePerFrame);
    chunks.reserve(totalFrames);

    for (uint32_t frameIndex = 0; frameIndex < totalFrames; ++frameIndex)
    {
        const size_t payloadOffset = static_cast<size_t>(frameIndex) * payloadSizePerFrame;
        const size_t payloadLen = std::min(payloadSizePerFrame, data.size() - payloadOffset);

        std::vector<uint8_t> frameData(kFrameByteCapacity, 0);
        auto writeUint32LE = [&frameData](size_t offset, uint32_t value)
        {
            frameData[offset + 0] = static_cast<uint8_t>(value & 0xFF);
            frameData[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
            frameData[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
            frameData[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
        };

        writeUint32LE(0, kFrameMagic);
        writeUint32LE(4, frameIndex);
        writeUint32LE(8, totalFrames);
        writeUint32LE(12, static_cast<uint32_t>(data.size()));
        writeUint32LE(16, static_cast<uint32_t>(payloadOffset));
        writeUint32LE(20, static_cast<uint32_t>(payloadLen));
        writeUint32LE(24, ComputeCrc32(data.data() + payloadOffset, payloadLen));
        std::copy(data.begin() + payloadOffset, data.begin() + payloadOffset + payloadLen,
                  frameData.begin() + static_cast<std::ptrdiff_t>(kFrameHeaderSize));
        chunks.push_back(std::move(frameData));
    }
    return chunks;
}

void CustomSymbolCodec::SaveFrameAsPpm(const std::vector<uint8_t>& bytes, const fs::path& filePath)
{
    std::vector<uint8_t> modules(kCodeModules * kCodeModules, 0);

    for (int i = 0; i < kCodeModules; ++i)
    {
        modules[i] = 1;
        modules[(kCodeModules - 1) * kCodeModules + i] = 1;
        modules[i * kCodeModules] = 1;
        modules[i * kCodeModules + (kCodeModules - 1)] = 1;
    }

    DrawFinder(modules, 2, 2);
    DrawFinder(modules, kCodeModules - 9, 2);
    DrawFinder(modules, 2, kCodeModules - 9);

    for (int i = 0; i < kCodeModules; ++i)
    {
        if (!IsReservedModule(i, 10))
            modules[10 * kCodeModules + i] = (i % 2 == 0) ? 1 : 0;
        if (!IsReservedModule(10, i))
            modules[i * kCodeModules + 10] = (i % 2 == 0) ? 1 : 0;
    }

    size_t bitPos = 0;
    for (int y = 0; y < kCodeModules; ++y)
    {
        for (int x = 0; x < kCodeModules; ++x)
        {
            if (IsReservedModule(x, y))
                continue;

            const size_t byteIdx = bitPos / 8;
            const int bitInByte = 7 - static_cast<int>(bitPos % 8);
            const uint8_t bit = (byteIdx < bytes.size()) ? static_cast<uint8_t>((bytes[byteIdx] >> bitInByte) & 0x01) : 0;
            modules[y * kCodeModules + x] = bit;
            ++bitPos;
        }
    }

    std::ofstream ofs(filePath, std::ios::binary);
    const int fullModules = kCodeModules + 2 * kQuietZoneModules;
    const int imageSize = fullModules * kModuleScale;
    ofs << "P5\n" << imageSize << " " << imageSize << "\n255\n";

    for (int py = 0; py < imageSize; ++py)
    {
        const int my = py / kModuleScale - kQuietZoneModules;
        for (int px = 0; px < imageSize; ++px)
        {
            const int mx = px / kModuleScale - kQuietZoneModules;
            uint8_t value = 255;
            if (mx >= 0 && mx < kCodeModules && my >= 0 && my < kCodeModules)
                value = modules[my * kCodeModules + mx] ? 0 : 255;
            ofs.put(static_cast<char>(value));
        }
    }
}

bool CustomSymbolCodec::IsReservedModule(int x, int y)
{
    if (x == 0 || y == 0 || x == kCodeModules - 1 || y == kCodeModules - 1 || x == 10 || y == 10)
        return true;

    const bool topLeftFinder = (x >= 1 && x <= 9 && y >= 1 && y <= 9);
    const bool topRightFinder = (x >= kCodeModules - 10 && x <= kCodeModules - 2 && y >= 1 && y <= 9);
    const bool bottomLeftFinder = (x >= 1 && x <= 9 && y >= kCodeModules - 10 && y <= kCodeModules - 2);
    if (topLeftFinder || topRightFinder || bottomLeftFinder)
        return true;

    return false;
}

void CustomSymbolCodec::DrawFinder(std::vector<uint8_t>& modules, int x0, int y0)
{
    for (int y = 0; y < 7; ++y)
    {
        for (int x = 0; x < 7; ++x)
        {
            const bool border = (x == 0 || x == 6 || y == 0 || y == 6);
            const bool center = (x >= 2 && x <= 4 && y >= 2 && y <= 4);
            modules[(y0 + y) * kCodeModules + (x0 + x)] = (border || center) ? 1 : 0;
        }
    }
}

uint32_t CustomSymbolCodec::ComputeCrc32(const uint8_t* data, size_t size)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
    }
    return ~crc;
}
