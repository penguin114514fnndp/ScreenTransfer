#pragma once
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

class QrVideoEncoder
{
public:
    bool Init(fs::path inputFile, fs::path outputVideo, int durationMs = 1000, fs::path ffmpegExe = "tools/ffmpeg.exe");
    int Encode();

    // 带宽报告
    void DisplayBandwidthReport();

private:
    fs::path mInputFile;  // 输入二进制文件路径
    fs::path mOutputVideo;  // 输出视频文件路径
    int mDurationMs;  // 视频持续时间
    fs::path mFfmpegExe;  // ffmpeg.exe路径
    static constexpr int kFixedFps = 20;  // 固定输出帧率

    bool ValidateInput() const;  // 验证输入参数的有效性
    std::vector<unsigned char> ReadAllBytes();  // 读取图片
    bool BuildVideoFromFrames(const fs::path& framePattern);  // 使用ffmpeg合成视频
};