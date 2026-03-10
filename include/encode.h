#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cstdint>
#include <sstream>
#include "qrcodegen.hpp"

using namespace std;
using qrcodegen::QrCode;
namespace fs = filesystem;

class QrVideoEncoder
{
public:
    bool Init(fs::path inputFile, fs::path outputVideo, int durationMs = 1000, int fps = 20, int LimitPerFrame = 1040, fs::path ffmpegExe = "tools/ffmpeg.exe")
    {
        mInputFile = inputFile;
        mOutputVideo = outputVideo;
        mDurationMs = durationMs;
        mFps = fps;
        mLimitPerFrame = LimitPerFrame;
        mFfmpegExe = ffmpegExe;

        if (!ValidateInput())  return false;
        return true;
    }

    int Encode()
    {
        // 读取输入文件
        const vector<unsigned char> raw = ReadAllBytes();
        if (raw.empty())
        {
            cerr << "Error: Failed to read input file or file is empty: " << mInputFile << "\n";
            return 3;
        }

        auto frames = CreateFrames(raw);
        
        fs::remove_all("output/frames");  // 清理旧帧数据
        fs::create_directories("output/frames");

        // 生成二维码图像并保存
        for (size_t i = 0; i < frames.size(); ++i)
        {
            const QrCode qr = QrCode::encodeBinary(frames[i], QrCode::Ecc::LOW);  // 使用 Ecc::LOW 提高容量
            string path = "output/frames/frame_" + to_string(i) + ".ppm";    
            SaveQrCodeAsPpm(qr, path);
        }

        // 使用ffmpeg将帧合成为视频
        if (!BuildVideoFromFrames("output/frames/frame_%d.ppm"))
        {
            cerr << "Error: Failed to build video from frames using ffmpeg.\n";
            return 4;
        }

        cout << "Video encoding completed successfully: " << mOutputVideo << "\n";
        return 0;
    }

    // 带宽报告
    void DisplayBandwidthReport()
    {
        // 基本参数计算
        size_t fileSize = fs::file_size(mInputFile);  // 输入文件大小（字节）
        double totalBandwidthKB = (mLimitPerFrame * mFps) / 1024.0;  // 每秒钟的总带宽（KB/s）
        double effectiveBandwidthKB = ((mLimitPerFrame - 4) * mFps) / 1024.0;  // 每秒有效带宽（KB/s）
        size_t totalFrames = (fileSize + (mLimitPerFrame - 5)) / (mLimitPerFrame - 4);  // 总帧数
        double durationSec = (double)totalFrames / mFps;  // 视频时长
        double requiredBandwidthKB = (fileSize / 1024.0) / (mDurationMs / 1000.0);  // 满足时长必要带宽

        cout << "\n--- Transmission Report ---" << endl;
        cout << "File Size:          " << fileSize / 1024.0 << " KB" << endl;
        cout << "QR Capacity (limit): " << mLimitPerFrame << " bytes/frame" << endl;
        cout << "Frame Rate (FPS):   " << mFps << endl;
        cout << "---------------------------" << endl;
        cout << "Raw Bandwidth:      " << totalBandwidthKB << " KB/s" << endl;
        cout << "Effective Goodput:  " << effectiveBandwidthKB << " KB/s (Target)" << endl;
        cout << "Estimated Duration: " << durationSec << " s (" << (durationSec * 1000 <= mDurationMs ? "PASS" : "FAIL") << ")" << endl;
        cout << "Required Bandwidth: " << requiredBandwidthKB << " KB/s (to meet max_ms)" << endl;
        cout << "---------------------------\n" << endl;
    }

private:
    fs::path mInputFile;  // 输入二进制文件路径
    fs::path mOutputVideo;  // 输出视频文件路径
    int mDurationMs;  // 视频持续时间
    int mFps;  // 视频帧率
    int mLimitPerFrame;  // 每帧最大字节数
    fs::path mFfmpegExe;  // ffmpeg.exe路径

    // 验证输入参数的有效性
    bool ValidateInput() const
    {
        if (mDurationMs <= 0)
        {
            cerr << "Error: Duration must be positive integers.\n";
            return false;
        }
        if (!fs::exists(mInputFile))
        {
            cerr << "Error: Input file not found: " << mInputFile << "\n";
            return false;
        }
        if (!fs::exists(mFfmpegExe))
        {
            cerr << "Error: ffmpeg executable not found: " << mFfmpegExe << "\n";
            return false;
        }
        return true;
    }

    // 读取图片
    vector<unsigned char> ReadAllBytes()
    {
        std::ifstream ifs(mInputFile, std::ios::binary);
        if (!ifs) return {};
        return std::vector<unsigned char>(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    }

    // 将输入数据打包成帧数据
    vector<vector<uint8_t>> CreateFrames(const vector<uint8_t> &data)
    {
        vector<vector<uint8_t>> chunks;
        chunks.reserve((data.size() + mLimitPerFrame - 5) / (mLimitPerFrame - 4)); // 预分配空间
    
        for (size_t i = 0; i < data.size(); i += (mLimitPerFrame - 4))
        {
            // 每帧前 4 字节放序号，后面接数据
            auto& f = chunks.emplace_back(4); 
            *(uint32_t*)f.data() = (uint32_t)chunks.size() - 1; 
            f.insert(f.end(), data.begin() + i, data.begin() + min(i + mLimitPerFrame - 4, data.size()));
        }
        return chunks;
    }

    // 储存二维码
    void SaveQrCodeAsPpm(const QrCode &qr, const fs::path &filePath, int scale = 10)
    {
        ofstream ofs(filePath, ios::binary);
        const int size = qr.getSize() * scale;

        ofs << "P5\n" << size << " " << size << "\n255\n";
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                ofs.put(qr.getModule(x / scale, y / scale) ? 0 : 255);  // 黑色为0，白色为255
    }

    // 使用ffmpeg合成视频
    bool  BuildVideoFromFrames(const fs::path &framePattern)
    {
        fs::path ffmpegPath = mFfmpegExe.make_preferred();
        fs::path outputPath = mOutputVideo.make_preferred();

        ostringstream inner;
        inner << '"' << ffmpegPath.string() << '"'
              << " -y"  // 覆盖输出文件
              << " -framerate " << mFps
              << " -i \"" << framePattern.string() << "\""
              << " -vf \"pad=ceil(iw/2)*2:ceil(ih/2)*2:color=white\""
              << " -c:v libx264 -preset ultrafast -crf 0 -tune stillimage -pix_fmt yuv420p" // 视频编码参数
              << " \"" << outputPath.string() << "\"";

        ostringstream cmd;
        cmd << "cmd /d /s /c \"" << inner.str() << "\"";

        cout << "Running: " << inner.str() << "\n";
        return system(cmd.str().c_str()) == 0;  // 返回cmd状态码
    }
};