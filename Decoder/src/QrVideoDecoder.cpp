#include "QrVideoDecoder.h"
#include "QrDetector.h"
#include <opencv2/opencv.hpp>
#include <iomanip>

bool QrVideoDecoder::Init(const fs::path& inputVideo, const fs::path& outputBin)
{
    mInputVideo = inputVideo;
    mOutputBin = outputBin;
    mFrames.clear();
    mPayloadSize = 0;

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
    int maxFrameNumber = 0;
    int decodedFrames = 0;
    int totalFrames = 0;

    while (cap.read(frame) && !frame.empty())
    {
        totalFrames++;

        // 检测二维码
        std::vector<uint8_t> qrRawData = QrDetector::Detect(frame);
        if (qrRawData.empty())
            continue;

        // 解析帧号和负载
        FrameData frameData;
        if (!ParseFrameData(qrRawData, frameData))
            continue;

        // 过滤重复帧
        if (mFrames.find(frameData.frameNumber) != mFrames.end())
            continue;

        // 第一次检测到有效帧时，设置payload大小
        if (mPayloadSize == 0)
            mPayloadSize = frameData.payload.size();

        maxFrameNumber = std::max(maxFrameNumber, frameData.frameNumber);
        mFrames[frameData.frameNumber] = std::move(frameData);
        decodedFrames++;
        std::cout << "\rDecoded frame " << decodedFrames << ": #" << mFrames[frameData.frameNumber].frameNumber << " (" << mFrames[frameData.frameNumber].payload.size() << " bytes)";

        // 进度显示
        if (totalFrames % 100 == 0)
            std::cout << "Progress: " << totalFrames << " frames\n";
    }
    cap.release();

    if (mFrames.empty())
    {
        std::cerr << "Error: No QR frames decoded\n";
        return 2;
    }

    // 合并帧数据
    mDecodedData = AssembleFrames(maxFrameNumber);

    // 保存结果
    if (!WriteFile(mOutputBin, mDecodedData))
    {
        std::cerr << "Error: Failed to write output\n";
        return 3;
    }

    std::cout << "Decoded: " << decodedFrames << "/" << totalFrames << " frames"
              << " -> " << mDecodedData.size() << " bytes\n";
    return 0;
}

bool QrVideoDecoder::ParseFrameData(const std::vector<uint8_t>& qrRawData, FrameData& frameData)
{
    if (qrRawData.size() < kFrameHeaderSize)
        return false;

    // 4字节小端序帧号
    frameData.frameNumber = qrRawData[0] | (qrRawData[1] << 8) | (qrRawData[2] << 16) | (qrRawData[3] << 24);
    frameData.payload.assign(qrRawData.begin() + kFrameHeaderSize, qrRawData.end());
    return true;
}

std::vector<uint8_t> QrVideoDecoder::AssembleFrames(int maxFrameNumber)
{
    std::vector<uint8_t> result;
    result.reserve((maxFrameNumber + 1) * mPayloadSize);

    // 按序号合并，缺失的帧用0x00填充
    for (int i = 0; i <= maxFrameNumber; i++)
    {
        auto it = mFrames.find(i);
        if (it != mFrames.end())
            result.insert(result.end(), it->second.payload.begin(), it->second.payload.end());
        else if (mPayloadSize > 0)
            result.insert(result.end(), mPayloadSize, 0x00);
    }
    return result;
}

bool QrVideoDecoder::WriteFile(const fs::path& path, const std::vector<uint8_t>& data)
{
    try
    {
        std::ofstream f(path, std::ios::binary);
        if (!f)
            return false;
        f.write((const char*)data.data(), data.size());
        return true;
    }
    catch (...)
    {
        return false;
    }
}
