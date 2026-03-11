#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "qrcodegen.hpp"

namespace fs = std::filesystem;
using qrcodegen::QrCode;

class QrVideoEncoder
{
public:
    bool Init(fs::path inputFile,
              fs::path outputVideo,
              int durationMs = 1000,
              int fps = 20,
              int limitPerFrame = 1040,
              fs::path ffmpegExe = "tools/ffmpeg.exe")
    {
        mInputFile = std::move(inputFile);
        mOutputVideo = std::move(outputVideo);
        mDurationMs = durationMs;
        mFps = fps;
        mLimitPerFrame = limitPerFrame;
        mFfmpegExe = std::move(ffmpegExe);

        if (!ValidateInput())
            return false;
        return true;
    }

    int Encode()
    {
        const std::vector<uint8_t> raw = ReadAllBytes();
        if (raw.empty())
        {
            std::cerr << "Error: Failed to read input file or file is empty: " << mInputFile << "\n";
            return 3;
        }

        const uint32_t fileCrc = Crc32(raw.data(), raw.size());
        const std::vector<std::string> frames = CreateFrames(raw, fileCrc);
        if (frames.empty())
        {
            std::cerr << "Error: Failed to create frames (check frame size limits).\n";
            return 4;
        }

        fs::remove_all("output/frames");
        fs::create_directories("output/frames");

        const size_t repeat = ComputeRepeat(frames.size());
        size_t outIndex = 0;

        for (size_t i = 0; i < frames.size(); ++i)
        {
            const std::vector<uint8_t> qrData(frames[i].begin(), frames[i].end());
            const QrCode qr = QrCode::encodeBinary(qrData, QrCode::Ecc::LOW);

            for (size_t r = 0; r < repeat; ++r)
            {
                const std::string path = "output/frames/frame_" + std::to_string(outIndex++) + ".ppm";
                SaveQrCodeAsPpm(qr, path);
            }
        }

        if (!BuildVideoFromFrames("output/frames/frame_%d.ppm"))
        {
            std::cerr << "Error: Failed to build video from frames using ffmpeg.\n";
            return 5;
        }

        std::cout << "Video encoding completed successfully: " << mOutputVideo << "\n";
        return 0;
    }

    void DisplayBandwidthReport()
    {
        const size_t fileSize = fs::file_size(mInputFile);
        const size_t headerSize = kHeaderSize;
        const size_t maxRaw = (static_cast<size_t>(mLimitPerFrame) / 4) * 3;
        const size_t chunkSize = maxRaw > headerSize ? (maxRaw - headerSize) : 0;
        const size_t totalFrames = chunkSize == 0 ? 0 : (fileSize + chunkSize - 1) / chunkSize;
        const size_t repeat = ComputeRepeat(totalFrames);

        const double dataFps = repeat == 0 ? 0.0 : (static_cast<double>(mFps) / repeat);
        const double rawBandwidthKB = (mLimitPerFrame * dataFps) / 1024.0;
        const double effectiveGoodputKB = (chunkSize * dataFps) / 1024.0;
        const double durationSec = dataFps <= 0.0 ? 0.0 : (totalFrames / dataFps);
        const double requiredBandwidthKB = (fileSize / 1024.0) / (mDurationMs / 1000.0);

        std::cout << "\n--- Transmission Report ---\n";
        std::cout << "File Size:          " << fileSize / 1024.0 << " KB\n";
        std::cout << "QR Payload Limit:   " << mLimitPerFrame << " bytes/frame\n";
        std::cout << "Chunk Size:         " << chunkSize << " bytes/frame\n";
        std::cout << "Frame Rate (FPS):   " << mFps << "\n";
        std::cout << "Repeat Factor:      " << repeat << "\n";
        std::cout << "---------------------------\n";
        std::cout << "Raw Bandwidth:      " << rawBandwidthKB << " KB/s\n";
        std::cout << "Effective Goodput:  " << effectiveGoodputKB << " KB/s (Target)\n";
        std::cout << "Estimated Duration: " << durationSec << " s (" << (durationSec * 1000 <= mDurationMs ? "PASS" : "FAIL") << ")\n";
        std::cout << "Required Bandwidth: " << requiredBandwidthKB << " KB/s (to meet max_ms)\n";
        std::cout << "---------------------------\n\n";
    }

private:
    fs::path mInputFile;
    fs::path mOutputVideo;
    int mDurationMs = 0;
    int mFps = 20;
    int mLimitPerFrame = 1040;
    fs::path mFfmpegExe;

    static constexpr uint32_t kMagic = 0x31545651u; // "QVT1"
    static constexpr uint8_t kVersion = 1;
    static constexpr size_t kHeaderSize = 40;

    bool ValidateInput() const
    {
        if (mDurationMs <= 0)
        {
            std::cerr << "Error: Duration must be positive integers.\n";
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

    std::vector<uint8_t> ReadAllBytes()
    {
        std::ifstream ifs(mInputFile, std::ios::binary);
        if (!ifs)
            return {};
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    }

    static uint32_t Crc32(const uint8_t *data, size_t len)
    {
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i)
        {
            crc ^= data[i];
            for (int j = 0; j < 8; ++j)
                crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
        return ~crc;
    }

    static std::string Base64Encode(const std::vector<uint8_t> &data)
    {
        static const char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((data.size() + 2) / 3) * 4);

        int val = 0;
        int valb = -6;
        for (uint8_t c : data)
        {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0)
            {
                out.push_back(kTable[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6)
            out.push_back(kTable[((val << 8) >> (valb + 8)) & 0x3F]);
        while (out.size() % 4)
            out.push_back('=');
        return out;
    }

    static void AppendLe(std::vector<uint8_t> &buf, uint64_t value, size_t bytes)
    {
        for (size_t i = 0; i < bytes; ++i)
            buf.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
    }

    std::vector<std::string> CreateFrames(const std::vector<uint8_t> &data, uint32_t fileCrc)
    {
        const size_t maxRaw = (static_cast<size_t>(mLimitPerFrame) / 4) * 3;
        if (maxRaw <= kHeaderSize)
            return {};

        const size_t chunkSize = maxRaw - kHeaderSize;
        const uint64_t fileSize = data.size();
        const uint32_t totalFrames = static_cast<uint32_t>((fileSize + chunkSize - 1) / chunkSize);

        std::vector<std::string> frames;
        frames.reserve(totalFrames);

        for (uint32_t i = 0; i < totalFrames; ++i)
        {
            const size_t offset = static_cast<size_t>(i) * chunkSize;
            const size_t payloadLen = std::min(chunkSize, data.size() - offset);
            const uint32_t payloadCrc = Crc32(data.data() + offset, payloadLen);

            std::vector<uint8_t> frame;
            frame.reserve(kHeaderSize + payloadLen);

            AppendLe(frame, kMagic, 4);
            frame.push_back(kVersion);
            const uint8_t flags = (i + 1 == totalFrames) ? 0x01 : 0x00;
            frame.push_back(flags);
            AppendLe(frame, 0, 2);
            AppendLe(frame, fileSize, 8);
            AppendLe(frame, fileCrc, 4);
            AppendLe(frame, chunkSize, 4);
            AppendLe(frame, totalFrames, 4);
            AppendLe(frame, i, 4);
            AppendLe(frame, payloadLen, 4);
            AppendLe(frame, payloadCrc, 4);
            frame.insert(frame.end(), data.begin() + offset, data.begin() + offset + payloadLen);

            frames.push_back(Base64Encode(frame));
        }

        return frames;
    }

    size_t ComputeRepeat(size_t dataFrames) const
    {
        if (dataFrames == 0)
            return 1;
        const double desiredFrames = (mDurationMs / 1000.0) * mFps;
        if (desiredFrames <= dataFrames)
            return 1;
        return static_cast<size_t>(std::ceil(desiredFrames / dataFrames));
    }

    void SaveQrCodeAsPpm(const QrCode &qr, const fs::path &filePath, int scale = 10)
    {
        std::ofstream ofs(filePath, std::ios::binary);
        const int size = qr.getSize() * scale;

        ofs << "P5\n" << size << " " << size << "\n255\n";
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                ofs.put(qr.getModule(x / scale, y / scale) ? 0 : 255);
    }

    bool BuildVideoFromFrames(const fs::path &framePattern)
    {
        fs::path ffmpegPath = mFfmpegExe.make_preferred();
        fs::path outputPath = mOutputVideo.make_preferred();

        std::ostringstream inner;
        inner << '"' << ffmpegPath.string() << '"'
              << " -y"
              << " -framerate " << mFps
              << " -i \"" << framePattern.string() << "\""
              << " -vf \"pad=ceil(iw/2)*2:ceil(ih/2)*2:color=white\""
              << " -c:v libx264 -preset ultrafast -crf 0 -tune stillimage -pix_fmt yuv420p"
              << " \"" << outputPath.string() << "\"";

        std::ostringstream cmd;
        cmd << "cmd /d /s /c \"" << inner.str() << "\"";

        std::cout << "Running: " << inner.str() << "\n";
        return system(cmd.str().c_str()) == 0;
    }
};
