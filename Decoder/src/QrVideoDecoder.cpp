#include "QrVideoDecoder.h"
#include <iomanip>

// 初始化
bool QrVideoDecoder::Init(const fs::path& inputVideo, const fs::path& outputBin, const fs::path& outputValidity, const fs::path& referenceBin)
{
    mInputVideo = inputVideo;
    mOutputBin = outputBin;
    mOutputValidity = outputValidity;
    mPayloadSize = 0;
    mReferenceData.clear();
    mFrames.clear();

    if (!fs::exists(inputVideo))
    {
        std::cerr << "Error: Input video file not found: " << inputVideo << "\n";
        return false;
    }
    
    const auto outputDir = outputBin.parent_path();
    if (!outputDir.empty() && !fs::exists(outputDir))
        fs::create_directories(outputDir);

    // 读取参考文件
    if (!referenceBin.empty())
    {
        std::ifstream refFile(referenceBin, std::ios::binary);
        if (!refFile)
        {
            std::cerr << "Error: Cannot open reference source file: " << referenceBin << "\n";
            return false;
        }

        refFile.seekg(0, std::ios::end);
        const auto size = refFile.tellg();
        refFile.seekg(0, std::ios::beg);

        if (size > 0)
        {
            mReferenceData.resize(static_cast<std::size_t>(size));
            refFile.read(reinterpret_cast<char*>(mReferenceData.data()), size);
        }
    }
    return true;
}

// 读取QR code并解析数据
int QrVideoDecoder::Decode()
{
    cv::VideoCapture cap(mInputVideo.string());
    if (!cap.isOpened())
    {
        std::cerr << "Error: Cannot open video file: " << mInputVideo << "\n";
        return 1;
    }

    cv::Mat frame;
    int maxFrameNumber = 0;
    while (cap.read(frame) && !frame.empty())
    {
        std::vector<uint8_t> qrData;
        FrameData frameData;
        
        // 识别QR码 解析数据
        std::cout << "Processing frame " << std::setw(5) << std::setfill('0') << (mFrames.size() + 1) << std::endl;
        if (!RecognizeQrCode(frame, qrData) || !ParseFrameData(qrData, frameData))
            continue;
        // 检查重复
        if (mFrames.find(frameData.frameNumber) != mFrames.end())
            continue;

        if (mPayloadSize == 0)
            mPayloadSize = frameData.payload.size();
        
        maxFrameNumber = std::max(maxFrameNumber, frameData.frameNumber);
        mFrames[frameData.frameNumber] = std::move(frameData);
    }
    cap.release();
    if (mFrames.empty())
    {
        std::cerr << "Error: No valid QR frame recognized in video.\n";
        return 2;
    }

    std::vector<uint8_t> completeData = AssembleCompleteData(maxFrameNumber); // 生成完整数据
    std::vector<uint8_t> validityData = GenerateValidity(completeData);  // 生成有效性标记
    if (!WriteFile(mOutputBin, completeData) || !WriteFile(mOutputValidity, validityData))
    {
        std::cerr << "Error: Failed to write output files.\n";
        return 3;
    }
    return 0;
}

// QR码识别
bool QrVideoDecoder::RecognizeQrCode(const cv::Mat& frame, std::vector<uint8_t>& decodedData)
{
    std::cout << "Recognizing QR code in frame " << std::endl;
    const int kMaxDecodeSide = 1200;  // 最大缩放尺寸
        
    // 如果帧过大，缩放
    cv::Mat toProcess = frame;
    int maxSide = std::max(frame.rows, frame.cols);
    if (maxSide > kMaxDecodeSide)
    {
        double scale = static_cast<double>(kMaxDecodeSide) / maxSide;
        cv::resize(frame, toProcess, cv::Size(), scale, scale, cv::INTER_AREA);
    }
    cv::cvtColor(toProcess, toProcess, cv::COLOR_BGR2GRAY);
    cv::threshold(toProcess, toProcess, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    
    std::cout << "Detecting and decoding QR code..." << std::endl;
    // 识别QR码
    cv::QRCodeDetector detector;
    std::string data = detector.detectAndDecode(toProcess);
    if (data.empty())   
        return false;       

    decodedData.assign(reinterpret_cast<const uint8_t*>(data.data()), reinterpret_cast<const uint8_t*>(data.data()) + data.size());
    return true;
}

// 帧数据解析
bool QrVideoDecoder::ParseFrameData(const std::vector<uint8_t>& qrData, FrameData& frameData)
{
    std::cout << "Parsing frame data " << std::endl;
    if (qrData.size() < kFrameHeaderSize)
        return false;

    // 解析4字节小端序序号
    frameData.frameNumber = (uint32_t)qrData[0] | ((uint32_t)qrData[1] << 8) | ((uint32_t)qrData[2] << 16) | ((uint32_t)qrData[3] << 24);
    frameData.payload.assign(qrData.begin() + kFrameHeaderSize, qrData.end());
    return true;
}

// 合并数据，填充缺失帧
std::vector<uint8_t> QrVideoDecoder::AssembleCompleteData(const int maxFrameNumber)
{
    std::vector<uint8_t> completeData;
    completeData.reserve((maxFrameNumber + 1) * mPayloadSize);
    
    for (uint32_t i = 0; i <= maxFrameNumber; ++i)
    {
        auto it = mFrames.find(i);
        if (it != mFrames.end())
            completeData.insert(completeData.end(), it->second.payload.begin(), it->second.payload.end());
        else if (mPayloadSize > 0)
            completeData.insert(completeData.end(), mPayloadSize, 0x00);
    }
    return completeData;
}

// 对比数据生成有效性标记 (参考文件)
std::vector<uint8_t> QrVideoDecoder::GenerateValidity(const std::vector<uint8_t>& completeData)
{
    std::vector<uint8_t> validityData(completeData.size(), 0xFF);
    mValidDataSize = 0;
    mLostBits = 0;
    for (std::size_t i = 0; i < completeData.size(); ++i)
    {
        bool match = (i < mReferenceData.size()) && (completeData[i] == mReferenceData[i]);
        validityData[i] = match ? 0xFF : 0x00;
        (match ? mValidDataSize : mLostBits) += 8;
    }
    return validityData;
}

// 文件写入
bool QrVideoDecoder::WriteFile(const fs::path& path, const std::vector<uint8_t>& data)
{
    try
    {
        std::ofstream file(path, std::ios::binary);
        if (!file)
        {
            std::cerr << "Error: Cannot open file: " << path << "\n";
            return false;
        }
        file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error writing file: " << e.what() << "\n";
        return false;
    }
}

// 生成报告
void QrVideoDecoder::DisplayDecodingReport()
{
    int totalDataSize = mValidDataSize + mLostBits;
    std::cout << "Frames: " << mFrames.size() << "\n";
    std::cout << "Data bytes: " << totalDataSize / 8 << "\n";

    if (totalDataSize > 0)
    {
        const double validityRate = 100.0 * mValidDataSize / totalDataSize;
        std::cout << "Valid bits: " << std::fixed << std::setprecision(2) << validityRate << "%\n";
    }
}
