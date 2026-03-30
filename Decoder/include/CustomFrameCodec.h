#pragma once

#include <opencv2/opencv.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <unordered_map>

constexpr std::size_t kFrameHeaderSize = 28;
constexpr uint32_t kFrameMagic = 0x31445543; // "CUD1"
constexpr int kCodeModules = 121;

// 单个帧的数据
struct FrameData
{
    uint32_t frameNumber;               // 帧序号
    uint32_t payloadOffset = 0;         // 原始数据偏移
    std::vector<uint8_t> payload;       // 负载
    bool checksumOk = false;            // 负载CRC校验是否通过
    FrameData(uint32_t num = 0) : frameNumber(num) {}
};


class CustomFrameCodec
{
public:
    uint32_t mExpectedTotalFrames = 0;
    uint32_t mExpectedTotalSize = 0;
    uint32_t mCrcOkFrames = 0;
    uint32_t mCrcBadFrames = 0;

    bool DetectAndParseFrame(const cv::Mat& frame, FrameData& outFrame);
    static bool DetectRawPayload(const cv::Mat& frame, std::vector<uint8_t>& raw);
    bool ParseFrameData(const std::vector<uint8_t>& raw, FrameData& outFrame);
    bool AssembleFrames(const std::unordered_map<uint32_t, FrameData>& frames, std::vector<uint8_t>& outData) const;

private:
    static uint32_t ReadUint32LE(const std::vector<uint8_t>& data, std::size_t offset);
    static uint32_t ComputeCrc32(const uint8_t* data, std::size_t size);
};
