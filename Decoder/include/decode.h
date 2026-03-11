#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>

namespace fs = std::filesystem;

class QrVideoDecoder
{
public:
    bool Init(fs::path inputVideo,
              fs::path outputBin,
              fs::path outputValid,
              fs::path ffmpegExe = "tools/ffmpeg.exe",
              fs::path framesDir = "output/decoded_frames")
    {
        mInputVideo = std::move(inputVideo);
        mOutputBin = std::move(outputBin);
        mOutputValid = std::move(outputValid);
        mFfmpegExe = std::move(ffmpegExe);
        mFramesDir = std::move(framesDir);

        if (!ValidateInput())
            return false;
        return true;
    }

    int Decode()
    {
        if (!ExtractFrames())
        {
            std::cerr << "Error: Failed to extract frames using ffmpeg.\n";
            return 3;
        }

        const std::vector<fs::path> frames = CollectFrames();
        if (frames.empty())
        {
            std::cerr << "Error: No frames found after extraction.\n";
            return 4;
        }

        cv::QRCodeDetector detector;
        std::unordered_map<uint32_t, FrameData> payloadByIndex;
        HeaderInfo headerInfo;

        size_t decodedCount = 0;
        size_t failedCount = 0;
        size_t invalidCount = 0;

        for (const auto &path : frames)
        {
            std::string decodedText;
            if (!DecodeFrame(detector, path, decodedText))
            {
                ++failedCount;
                continue;
            }

            std::vector<uint8_t> raw;
            if (!Base64Decode(decodedText, raw))
            {
                ++failedCount;
                continue;
            }

            FrameInfo info;
            std::vector<uint8_t> payload;
            if (!ParseFrame(raw, info, payload))
            {
                ++invalidCount;
                continue;
            }

            if (!headerInfo.Seen)
            {
                headerInfo = HeaderInfo::From(info);
            }
            if (!headerInfo.Matches(info))
            {
                ++invalidCount;
                continue;
            }

            FrameData &slot = payloadByIndex[info.FrameIndex];
            if (slot.Valid)
            {
                ++decodedCount;
                continue;
            }

            if (info.PayloadCrc == Crc32(payload.data(), payload.size()))
            {
                slot.Payload = std::move(payload);
                slot.Valid = true;
            }
            else
            {
                if (!slot.Valid)
                {
                    slot.Payload = std::move(payload);
                    slot.Valid = false;
                }
                ++invalidCount;
                continue;
            }

            ++decodedCount;
        }

        if (!headerInfo.Seen)
        {
            std::cerr << "Error: No valid QR payloads decoded.\n";
            return 5;
        }

        const uint64_t fileSize = headerInfo.FileSize;
        const uint32_t totalFrames = headerInfo.TotalFrames;
        const uint32_t chunkSize = headerInfo.ChunkSize;

        std::vector<uint8_t> output(fileSize, 0x00);
        std::vector<uint8_t> valid(fileSize, 0x00);

        size_t filledFrames = 0;
        for (uint32_t i = 0; i < totalFrames; ++i)
        {
            const auto it = payloadByIndex.find(i);
            if (it == payloadByIndex.end())
                continue;
            if (!it->second.Valid)
                continue;

            const size_t offset = static_cast<size_t>(i) * chunkSize;
            if (offset >= fileSize)
                continue;

            const std::vector<uint8_t> &payload = it->second.Payload;
            const size_t copyLen = std::min(payload.size(), static_cast<size_t>(fileSize - offset));

            std::copy(payload.begin(), payload.begin() + copyLen, output.begin() + offset);
            std::fill(valid.begin() + offset, valid.begin() + offset + copyLen, 0xFF);
            ++filledFrames;
        }

        if (!WriteBinary(mOutputBin, output))
        {
            std::cerr << "Error: Failed to write output file: " << mOutputBin << "\n";
            return 7;
        }
        if (!WriteBinary(mOutputValid, valid))
        {
            std::cerr << "Error: Failed to write validity file: " << mOutputValid << "\n";
            return 8;
        }

        const uint32_t outCrc = Crc32(output.data(), output.size());
        const bool crcOk = (outCrc == headerInfo.FileCrc);

        std::cout << "Decoded frames: " << decodedCount << "\n";
        std::cout << "Invalid frames: " << invalidCount << "\n";
        std::cout << "Failed frames:  " << failedCount << "\n";
        std::cout << "Filled frames:  " << filledFrames << " / " << totalFrames << "\n";
        std::cout << "Output bytes:   " << output.size() << "\n";
        std::cout << "CRC32:          " << std::hex << headerInfo.FileCrc << " (expected) / " << outCrc << std::dec
                  << " (actual) - " << (crcOk ? "PASS" : "FAIL") << "\n";
        return 0;
    }

private:
    struct FrameInfo
    {
        uint64_t FileSize = 0;
        uint32_t FileCrc = 0;
        uint32_t ChunkSize = 0;
        uint32_t TotalFrames = 0;
        uint32_t FrameIndex = 0;
        uint32_t PayloadLen = 0;
        uint32_t PayloadCrc = 0;
    };

    struct HeaderInfo
    {
        bool Seen = false;
        uint64_t FileSize = 0;
        uint32_t FileCrc = 0;
        uint32_t ChunkSize = 0;
        uint32_t TotalFrames = 0;

        static HeaderInfo From(const FrameInfo &info)
        {
            HeaderInfo h;
            h.Seen = true;
            h.FileSize = info.FileSize;
            h.FileCrc = info.FileCrc;
            h.ChunkSize = info.ChunkSize;
            h.TotalFrames = info.TotalFrames;
            return h;
        }

        bool Matches(const FrameInfo &info) const
        {
            return info.FileSize == FileSize && info.FileCrc == FileCrc && info.ChunkSize == ChunkSize &&
                   info.TotalFrames == TotalFrames;
        }
    };

    struct FrameData
    {
        std::vector<uint8_t> Payload;
        bool Valid = false;
    };

    fs::path mInputVideo;
    fs::path mOutputBin;
    fs::path mOutputValid;
    fs::path mFfmpegExe;
    fs::path mFramesDir;

    static constexpr uint32_t kMagic = 0x31545651u;
    static constexpr uint8_t kVersion = 1;
    static constexpr size_t kHeaderSize = 40;

    bool ValidateInput() const
    {
        if (!fs::exists(mInputVideo))
        {
            std::cerr << "Error: Input video not found: " << mInputVideo << "\n";
            return false;
        }
        if (!fs::exists(mFfmpegExe))
        {
            std::cerr << "Error: ffmpeg executable not found: " << mFfmpegExe << "\n";
            return false;
        }
        return true;
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

    static bool Base64Decode(const std::string &input, std::vector<uint8_t> &out)
    {
        static const int kMap[256] = {
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
            52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
            -1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
            15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
            -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
            41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
        };

        int val = 0;
        int valb = -8;
        out.clear();
        out.reserve((input.size() / 4) * 3);

        for (unsigned char c : input)
        {
            if (std::isspace(c))
                continue;
            if (c == '=')
                break;
            int d = kMap[c];
            if (d == -1)
                return false;
            val = (val << 6) + d;
            valb += 6;
            if (valb >= 0)
            {
                out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return true;
    }

    bool ExtractFrames()
    {
        fs::remove_all(mFramesDir);
        fs::create_directories(mFramesDir);

        fs::path ffmpegPath = mFfmpegExe.make_preferred();
        fs::path inputPath = mInputVideo.make_preferred();
        fs::path framePattern = (mFramesDir / "frame_%d.png").make_preferred();

        std::ostringstream inner;
        inner << '"' << ffmpegPath.string() << '"'
              << " -y -i \"" << inputPath.string() << "\""
              << " -vsync 0"
              << " \"" << framePattern.string() << "\"";

        std::ostringstream cmd;
        cmd << "cmd /d /s /c \"" << inner.str() << "\"";

        std::cout << "Running: " << inner.str() << "\n";
        return system(cmd.str().c_str()) == 0;
    }

    std::vector<fs::path> CollectFrames() const
    {
        std::vector<std::pair<int, fs::path>> indexed;
        std::regex pattern(R"(frame_(\d+)\.)", std::regex::icase);

        for (const auto &entry : fs::directory_iterator(mFramesDir))
        {
            if (!entry.is_regular_file())
                continue;
            const std::string name = entry.path().filename().string();
            std::smatch match;
            if (!std::regex_search(name, match, pattern))
                continue;
            const int index = std::stoi(match[1].str());
            indexed.emplace_back(index, entry.path());
        }

        std::sort(indexed.begin(), indexed.end(),
                  [](const auto &a, const auto &b) { return a.first < b.first; });

        std::vector<fs::path> frames;
        frames.reserve(indexed.size());
        for (const auto &item : indexed)
            frames.push_back(item.second);
        return frames;
    }

    bool DecodeFrame(cv::QRCodeDetector &detector, const fs::path &imagePath, std::string &outText)
    {
        cv::Mat image = cv::imread(imagePath.string(), cv::IMREAD_GRAYSCALE);
        if (image.empty())
            return false;

        cv::Mat binary;
        cv::threshold(image, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

        outText = detector.detectAndDecode(binary);
        return !outText.empty();
    }

    static uint64_t ReadLe(const std::vector<uint8_t> &buf, size_t &offset, size_t bytes)
    {
        uint64_t value = 0;
        for (size_t i = 0; i < bytes; ++i)
            value |= static_cast<uint64_t>(buf[offset++]) << (8 * i);
        return value;
    }

    bool ParseFrame(const std::vector<uint8_t> &raw, FrameInfo &info, std::vector<uint8_t> &payload)
    {
        if (raw.size() < kHeaderSize)
            return false;

        size_t offset = 0;
        const uint32_t magic = static_cast<uint32_t>(ReadLe(raw, offset, 4));
        const uint8_t version = static_cast<uint8_t>(ReadLe(raw, offset, 1));
        offset += 1; // flags
        offset += 2; // reserved

        if (magic != kMagic || version != kVersion)
            return false;

        info.FileSize = ReadLe(raw, offset, 8);
        info.FileCrc = static_cast<uint32_t>(ReadLe(raw, offset, 4));
        info.ChunkSize = static_cast<uint32_t>(ReadLe(raw, offset, 4));
        info.TotalFrames = static_cast<uint32_t>(ReadLe(raw, offset, 4));
        info.FrameIndex = static_cast<uint32_t>(ReadLe(raw, offset, 4));
        info.PayloadLen = static_cast<uint32_t>(ReadLe(raw, offset, 4));
        info.PayloadCrc = static_cast<uint32_t>(ReadLe(raw, offset, 4));

        if (info.PayloadLen > info.ChunkSize)
            return false;
        if (info.FrameIndex >= info.TotalFrames)
            return false;
        if (raw.size() < kHeaderSize + info.PayloadLen)
            return false;

        payload.assign(raw.begin() + kHeaderSize, raw.begin() + kHeaderSize + info.PayloadLen);
        return true;
    }

    bool WriteBinary(const fs::path &path, const std::vector<uint8_t> &data) const
    {
        if (path.has_parent_path())
            fs::create_directories(path.parent_path());

        std::ofstream ofs(path, std::ios::binary);
        if (!ofs)
            return false;
        if (!data.empty())
            ofs.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
        return static_cast<bool>(ofs);
    }
};
