#include "QrVideoDecoder.h"
#include "CustomFrameCodec.h"
#include <fstream>
#include <iostream>
#include <opencv2/opencv.hpp>

bool QrVideoDecoder::Init(const fs::path& inputVideo, const fs::path& outputBin)
{
    mInputVideo = inputVideo;
    mOutputBin = outputBin;
    mCodec = CustomFrameCodec();
    mFrames.clear();
    mDecodedData.clear();

    if (!fs::exists(inputVideo))
    {
        std::cerr << "Error: Video not found: " << inputVideo << "\n";
        return false;
    }
    return true;
}

int QrVideoDecoder::Decode()
{
    cv::VideoCapture cap(mInputVideo.string());
    if (!cap.isOpened())
    {
        std::cerr << "Error: Cannot open video\n";
        return 1;
    }
    
    cv::Mat frame;
    int totalFrames = 0;

    while (cap.read(frame) && !frame.empty())
    {
        totalFrames++;

        // 识别+解码
        FrameData frameData;
        if (!mCodec.DetectAndParseFrame(frame, frameData))
            continue;

        // 过滤重复帧（保留CRC更好/长度更完整的数据）
        auto it = mFrames.find(frameData.frameNumber);
        if (it == mFrames.end())
            mFrames.emplace(frameData.frameNumber, std::move(frameData));
        else
        {
            const bool shouldReplace = (!it->second.checksumOk && frameData.checksumOk) ||
                                       (it->second.payload.size() < frameData.payload.size());
            if (shouldReplace)
                it->second = std::move(frameData);
        }
    }
    if (mFrames.empty())
    {
        std::cerr << "Error: No frames decoded\n";
        return 2;
    }

    // 合并帧数据
    if (!mCodec.AssembleFrames(mFrames, mDecodedData))
    {
        std::cerr << "Error: Invalid expected output size\n";
        return 3;
    }

    // 保存结果
    if (!WriteFile(mOutputBin, mDecodedData))
    {
        std::cerr << "Error: Failed to write output\n";
        return 4;
    }
    return 0;
}

bool QrVideoDecoder::WriteFile(const fs::path& path, const std::vector<uint8_t>& data)
{
    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;

    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(f);
}

void QrVideoDecoder::ReportDecodeResult(int scannedFrames)
{
    const uint32_t recovered = static_cast<uint32_t>(mFrames.size());
    const uint32_t expectedTotalFrames = mCodec.mExpectedTotalFrames;
    const uint32_t missing = (expectedTotalFrames > recovered) ? (expectedTotalFrames - recovered) : 0;

    std::cout << "Decoded frame coverage: " << recovered << "/" << expectedTotalFrames << "\n";
    std::cout << "CRC status: OK=" << mCodec.mCrcOkFrames << ", suspect=" << mCodec.mCrcBadFrames << "\n";
    if (missing > 0)
        std::cout << "Missing frames filled with zeros: " << missing << "\n";
    std::cout << "Decoded bytes: " << mDecodedData.size() << " (video frames scanned: " << scannedFrames << ")\n";
}