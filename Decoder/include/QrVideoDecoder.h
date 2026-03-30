#pragma once

#include "CustomFrameCodec.h"
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

class QrVideoDecoder
{
public:
    bool Init(const fs::path& inputVideo, const fs::path& outputBin);
    int Decode();
    void ReportDecodeResult(int scannedFrames);

private:
    CustomFrameCodec mCodec;
    fs::path mInputVideo;
    fs::path mOutputBin;
    std::unordered_map<uint32_t, FrameData> mFrames;
    std::vector<uint8_t> mDecodedData;
    bool WriteFile(const fs::path& path, const std::vector<uint8_t>& data);
};
