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

// 帧数据结构
struct FrameData
{
    int frameNumber;           // 帧序号（从帧头读取）
    std::vector<uint8_t> payload;   // 负载数据
    FrameData(int num = 0) : frameNumber(num) {}
};

class QrVideoDecoder
{
public:
    QrVideoDecoder() {}
    bool Init(const fs::path& inputVideo, const fs::path& outputBin, const fs::path& outputValidity,
              const fs::path& referenceBin = {});
    int Decode();
    void DisplayDecodingReport();

private:
    fs::path mInputVideo;
    fs::path mOutputBin;
    fs::path mOutputValidity;

    std::map<uint32_t, FrameData> mFrames;       // 按序号存储识别到的帧
    std::size_t mValidDataSize;                  // 有效数据量
    std::size_t mLostBits;                       // 丢失比特数
    std::size_t mPayloadSize;                    // 单帧负载长度
    std::vector<uint8_t> mReferenceData;         // 源文件字节流
    std::ofstream mDiagnosticFile;               // 诊断日志文件
    int mTotalFrames;                            // 处理过的总帧数
    int mDetectedFrames;                         // 成功识别的帧数

    bool RecognizeQrCode(const cv::Mat& frame, std::vector<uint8_t>& decodedData, int frameIndex);
    bool ParseFrameData(const std::vector<uint8_t>& qrData, FrameData& frameData);
    std::vector<uint8_t> AssembleCompleteData(const int maxFrameNumber);
    std::vector<uint8_t> GenerateValidity(const std::vector<uint8_t>& completeData);
    bool WriteFile(const fs::path& path, const std::vector<uint8_t>& data);
    void LogDiagnostic(const std::string& message);
};
