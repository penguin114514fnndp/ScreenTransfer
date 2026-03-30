#include "QrVideoEncoder.h"
#include "CustomSymbolCodec.h"
#include <fstream>
#include <iostream>
#include <sstream>

bool QrVideoEncoder::Init(fs::path inputFile, fs::path outputVideo, int durationMs, fs::path ffmpegExe)
{
    mInputFile = inputFile;
    mOutputVideo = outputVideo;
    mDurationMs = durationMs;
    mFfmpegExe = ffmpegExe;

    if (!ValidateInput())
        return false;
    return true;
}

int QrVideoEncoder::Encode()
{
    // 读取输入文件
    const std::vector<unsigned char> raw = ReadAllBytes();
    if (raw.empty())
    {
        std::cerr << "Error: Failed to read input file or file is empty: " << mInputFile << "\n";
        return 3;
    }

    // 将输入数据打包成帧数据
    auto frames = CustomSymbolCodec::CreateFrames(raw);
    if (frames.empty())
    {
        std::cerr << "Error: Failed to create custom frames.\n";
        return 5;
    }

    fs::remove_all("output/frames");  // 清理旧帧数据
    fs::create_directories("output/frames");

    // 生成二维码图像并保存
    for (size_t i = 0; i < frames.size(); ++i)
    {
        std::string path = "output/frames/frame_" + std::to_string(i) + ".ppm";
        CustomSymbolCodec::SaveFrameAsPpm(frames[i], path);
    }

    // 使用ffmpeg将帧合成为视频
    if (!BuildVideoFromFrames("output/frames/frame_%d.ppm"))
    {
        std::cerr << "Error: Failed to build video from frames using ffmpeg.\n";
        return 4;
    }

    std::cout << "Video encoding completed successfully: " << mOutputVideo << "\n";
    return 0;
}

void QrVideoEncoder::DisplayBandwidthReport()
{
    // 基本参数计算
    const size_t frameBytes = CustomSymbolCodec::FrameByteCapacity();
    const size_t payloadPerFrame = CustomSymbolCodec::PayloadCapacityPerFrame();
    if (payloadPerFrame == 0)
    {
        std::cerr << "Error: Invalid symbol capacity.\n";
        return;
    }

    const size_t fileSize = fs::file_size(mInputFile);
    const double totalBandwidthKB = (frameBytes * kFixedFps) / 1024.0;
    const double effectiveBandwidthKB = (payloadPerFrame * kFixedFps) / 1024.0;
    const size_t totalFrames = (fileSize + payloadPerFrame - 1) / payloadPerFrame;
    const double durationSec = static_cast<double>(totalFrames) / kFixedFps;
    const double requiredBandwidthKB = (fileSize / 1024.0) / (mDurationMs / 1000.0);

    std::cout << "\n--- Transmission Report ---" << std::endl;
    std::cout << "File Size:          " << fileSize / 1024.0 << " KB" << std::endl;
    std::cout << "Custom Capacity:     " << frameBytes << " bytes/frame" << std::endl;
    std::cout << "Frame Rate (FPS):   " << kFixedFps << std::endl;
    std::cout << "---------------------------" << std::endl;
    std::cout << "Raw Bandwidth:      " << totalBandwidthKB << " KB/s" << std::endl;
    std::cout << "Effective Goodput:  " << effectiveBandwidthKB << " KB/s (Target)" << std::endl;
    std::cout << "Estimated Duration: " << durationSec << " s (" << (durationSec * 1000 <= mDurationMs ? "PASS" : "FAIL") << ")" << std::endl;
    std::cout << "Required Bandwidth: " << requiredBandwidthKB << " KB/s (to meet max_ms)" << std::endl;
    std::cout << "---------------------------\n" << std::endl;
}

bool QrVideoEncoder::ValidateInput() const
{
    if (mDurationMs <= 0)
    {
        std::cerr << "Error: Duration must be positive integers.\n";
        return false;
    }
    if (CustomSymbolCodec::PayloadCapacityPerFrame() == 0)
    {
        std::cerr << "Error: Custom symbol payload capacity is zero.\n";
        return false;
    }
    if (!fs::exists(mInputFile))
    {
        std::cerr << "Error: Input file not found: " << mInputFile << "\n";
        return false;
    }
    if (!fs::exists(mFfmpegExe))
    {
        std::cerr << "Error: ffmpeg executable not found: " << mFfmpegExe << "\n";
        return false;
    }
    return true;
}

std::vector<unsigned char> QrVideoEncoder::ReadAllBytes()
{
    std::ifstream ifs(mInputFile, std::ios::binary);
    if (!ifs)
        return {};
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
}

bool QrVideoEncoder::BuildVideoFromFrames(const fs::path& framePattern)
{
    fs::path ffmpegPath = mFfmpegExe.make_preferred();
    fs::path outputPath = mOutputVideo.make_preferred();

    std::ostringstream inner;
    inner << '"' << ffmpegPath.string() << '"'
          << " -y"  // 覆盖输出文件
          << " -framerate " << kFixedFps
          << " -i \"" << framePattern.string() << "\""
          << " -c:v libx264rgb -preset veryslow -crf 0 -g 1 -bf 0 -pix_fmt rgb24"
          << " \"" << outputPath.string() << "\"";

    std::ostringstream cmd;
    cmd << "cmd /d /s /c \"" << inner.str() << "\"";

    std::cout << "Running: " << inner.str() << "\n";
    return system(cmd.str().c_str()) == 0;  // 返回cmd状态码
}
