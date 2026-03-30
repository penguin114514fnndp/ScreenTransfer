#include "CustomFrameCodec.h"
#include <algorithm>


bool IsReservedModule(int x, int y)
{
    if (x == 0 || y == 0 || x == kCodeModules - 1 || y == kCodeModules - 1 || x == 10 || y == 10)
        return true;

    const bool topLeftFinder = (x >= 1 && x <= 9 && y >= 1 && y <= 9);
    const bool topRightFinder = (x >= kCodeModules - 10 && x <= kCodeModules - 2 && y >= 1 && y <= 9);
    const bool bottomLeftFinder = (x >= 1 && x <= 9 && y >= kCodeModules - 10 && y <= kCodeModules - 2);
    return topLeftFinder || topRightFinder || bottomLeftFinder;
}

const std::vector<int>& DataModuleIndices()
{
    static const std::vector<int> indices = []
    {
        std::vector<int> idx;
        idx.reserve(kCodeModules * kCodeModules);
        for (int y = 0; y < kCodeModules; ++y)
            for (int x = 0; x < kCodeModules; ++x)
                if (!IsReservedModule(x, y))
                    idx.push_back(y * kCodeModules + x);
        return idx;
    }();
    return indices;
}

int CountBlackOnBorder(const cv::Mat& modules)
{
    int black = 0;
    for (int i = 0; i < kCodeModules; ++i)
    {
        black += (modules.at<uint8_t>(0, i) == 0) ? 1 : 0;
        black += (modules.at<uint8_t>(kCodeModules - 1, i) == 0) ? 1 : 0;
        black += (modules.at<uint8_t>(i, 0) == 0) ? 1 : 0;
        black += (modules.at<uint8_t>(i, kCodeModules - 1) == 0) ? 1 : 0;
    }
    return black;
}

bool CustomFrameCodec::DetectAndParseFrame(const cv::Mat& frame, FrameData& outFrame)
{
    std::vector<uint8_t> raw;
    if (!DetectRawPayload(frame, raw))
        return false;
    return ParseFrameData(raw, outFrame);
}

bool CustomFrameCodec::DetectRawPayload(const cv::Mat& frame, std::vector<uint8_t>& raw)
{
    if (frame.empty())
        return false;

    cv::Mat gray;
    if (frame.channels() == 3 || frame.channels() == 4)
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    else
        gray = frame;

    cv::Mat invBinary;
    cv::threshold(gray, invBinary, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    std::vector<cv::Point> points;
    cv::findNonZero(invBinary, points);
    if (points.empty())
        return false;

    const cv::Rect bbox = cv::boundingRect(points);
    if (bbox.width < 32 || bbox.height < 32)
        return false;

    const int side = std::max(bbox.width, bbox.height);
    const int cx = bbox.x + bbox.width / 2;
    const int cy = bbox.y + bbox.height / 2;
    const int x0 = std::clamp(cx - side / 2, 0, gray.cols - side);
    const int y0 = std::clamp(cy - side / 2, 0, gray.rows - side);
    const cv::Rect square(x0, y0, std::min(side, gray.cols - x0), std::min(side, gray.rows - y0));
    if (square.width < 32 || square.height < 32)
        return false;

    cv::Mat symbol = gray(square).clone();
    cv::resize(symbol, symbol, cv::Size(kCodeModules, kCodeModules), 0, 0, cv::INTER_AREA);
    cv::threshold(symbol, symbol, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    const int borderBlack = CountBlackOnBorder(symbol);
    const int borderTotal = kCodeModules * 4;
    if (borderBlack < borderTotal * 9 / 10)
        return false;

    const std::vector<int>& indices = DataModuleIndices();
    raw.assign(indices.size() / 8, 0);
    for (size_t bitPos = 0; bitPos < indices.size(); ++bitPos)
    {
        const int flat = indices[bitPos];
        if (symbol.at<uint8_t>(flat / kCodeModules, flat % kCodeModules) != 0)
            continue;

        const size_t byteIdx = bitPos / 8;
        const int bitInByte = 7 - static_cast<int>(bitPos % 8);
        raw[byteIdx] |= static_cast<uint8_t>(1u << bitInByte);
    }

    return !raw.empty();
}

bool CustomFrameCodec::ParseFrameData(const std::vector<uint8_t>& raw, FrameData& outFrame)
{
    if (raw.size() < kFrameHeaderSize)
        return false;

    const uint32_t magic = ReadUint32LE(raw, 0);
    if (magic != kFrameMagic)
        return false;

    const uint32_t frameNumber = ReadUint32LE(raw, 4);
    const uint32_t totalFrames = ReadUint32LE(raw, 8);
    const uint32_t totalSize = ReadUint32LE(raw, 12);
    const uint32_t payloadOffset = ReadUint32LE(raw, 16);
    const uint32_t payloadSize = ReadUint32LE(raw, 20);
    const uint32_t payloadCrc32 = ReadUint32LE(raw, 24);

    if (totalFrames == 0 || totalSize == 0)
        return false;
    if (frameNumber >= totalFrames)
        return false;
    if (payloadOffset >= totalSize)
        return false;

    if (mExpectedTotalFrames == 0)
    {
        mExpectedTotalFrames = totalFrames;
        mExpectedTotalSize = totalSize;
    }
    else if (mExpectedTotalFrames != totalFrames || mExpectedTotalSize != totalSize)
    {
        return false;
    }

    const uint32_t available = static_cast<uint32_t>(raw.size() - kFrameHeaderSize);
    const uint32_t lenFromFrame = std::min(payloadSize, available);
    const uint32_t room = totalSize - payloadOffset;
    const uint32_t payloadLen = std::min(lenFromFrame, room);

    outFrame.frameNumber = frameNumber;
    outFrame.payloadOffset = payloadOffset;
    outFrame.payload.assign(raw.begin() + kFrameHeaderSize,
                            raw.begin() + kFrameHeaderSize + payloadLen);

    const uint32_t crc = ComputeCrc32(outFrame.payload.data(), outFrame.payload.size());
    outFrame.checksumOk = (payloadLen == payloadSize) && (crc == payloadCrc32);
    if (outFrame.checksumOk)
        mCrcOkFrames++;
    else
        mCrcBadFrames++;
    return true;
}

bool CustomFrameCodec::AssembleFrames(const std::unordered_map<uint32_t, FrameData>& frames,
                                      std::vector<uint8_t>& outData) const
{
    if (mExpectedTotalSize == 0)
        return false;

    outData.assign(mExpectedTotalSize, 0);
    for (const auto& kv : frames)
    {
        const FrameData& frame = kv.second;
        if (frame.payloadOffset >= mExpectedTotalSize)
            continue;

        const uint32_t writable = mExpectedTotalSize - frame.payloadOffset;
        const uint32_t copyLen = std::min<uint32_t>(writable, static_cast<uint32_t>(frame.payload.size()));
        std::copy(frame.payload.begin(), frame.payload.begin() + copyLen, outData.begin() + frame.payloadOffset);
    }
    return true;
}

uint32_t CustomFrameCodec::ReadUint32LE(const std::vector<uint8_t>& data, std::size_t offset)
{
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
}

uint32_t CustomFrameCodec::ComputeCrc32(const uint8_t* data, std::size_t size)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
    }
    return ~crc;
}
