#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include <filesystem>
#include <opencv2/opencv.hpp>

namespace fs = std::filesystem;
constexpr std::size_t kFrameHeaderSize = 4;

// 单个帧的数据
struct FrameData
{
    int frameNumber;                    // 帧序号
    std::vector<uint8_t> payload;       // 负载
    FrameData(int num = 0) : frameNumber(num) {}
};

class QrVideoDecoder
{
public:
    QrVideoDecoder() {}
    bool Init(const fs::path& inputVideo, const fs::path& outputBin);
    int Decode();
    std::vector<uint8_t> GetDecodedData() const { return mDecodedData; }

private:
    fs::path mInputVideo;
    fs::path mOutputBin;
    std::map<uint32_t, FrameData> mFrames;
    std::size_t mPayloadSize = 0;
    std::vector<uint8_t> mDecodedData;

    bool ParseFrameData(const std::vector<uint8_t>& qrRawData, FrameData& frameData);
    std::vector<uint8_t> AssembleFrames(int maxFrameNumber);
    bool WriteFile(const fs::path& path, const std::vector<uint8_t>& data);
};
