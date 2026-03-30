#include "QrVideoDecoder.h"
#include <iomanip>
#include <cstdio>

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
        if (!RecognizeQrCode(frame, qrData, 0) || !ParseFrameData(qrData, frameData))
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

// QR码识别 - 多策略检测
// 使用pyzbar库识别QR码（首选方法）
bool QrVideoDecoder::DetectQrWithPyzbar(const cv::Mat& frame, std::vector<uint8_t>& decodedData, int frameIndex)
{
    try
    {
        // 保存帧为临时PPM文件
        fs::path tempDir = "output/temp_qr";
        if (!fs::exists(tempDir))
            fs::create_directories(tempDir);

        fs::path framePath = tempDir / ("frame_" + std::to_string(frameIndex) + ".png");

        // 使用PNG格式（更兼容）
        if (!cv::imwrite(framePath.string(), frame))
            return false;
        
        // 调用Python脚本: python decode_qr_pyzbar.py <frame_path>
        std::string command = "python decode_qr_pyzbar.py \"" + framePath.string() + "\" 2>nul";

        // 执行命令并捕获输出
        FILE* pipe = _popen(command.c_str(), "r");
        if (!pipe)
            return false;

        std::string output;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            output += buffer;
        }
        _pclose(pipe);

        // 清理临时文件
        try { fs::remove(framePath); } catch (...) {}

        // 解析JSON输出 查找 data 字段
        size_t dataPos = output.find("\"data\":");
        if (dataPos == std::string::npos)
            return false;

        size_t colonPos = output.find(":", dataPos);
        size_t quoteStart = output.find("\"", colonPos);
        if (quoteStart == std::string::npos)
            return false;

        size_t quoteEnd = output.find("\"", quoteStart + 1);
        if (quoteEnd == std::string::npos)
            return false;

        std::string hexData = output.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

        // 处理null值
        if (hexData == "null" || hexData.empty())
            return false;

        // 将十六进制字符串转换回二进制数据
        decodedData.clear();
        for (size_t i = 0; i < hexData.length(); i += 2)
        {
            if (i + 1 < hexData.length())
            {
                std::string byteStr = hexData.substr(i, 2);
                uint8_t byte = static_cast<uint8_t>(strtol(byteStr.c_str(), nullptr, 16));
                decodedData.push_back(byte);
            }
        }

        if (!decodedData.empty())
            return true;

        return false;
    }
    catch (const std::exception& e)
    {
        return false;
    }
}

bool QrVideoDecoder::RecognizeQrCode(const cv::Mat& frame, std::vector<uint8_t>& decodedData, int frameIndex)
{
    // 尝试使用pyzbar
    if (DetectQrWithPyzbar(frame, decodedData, frameIndex))
        return true;

    // 备选: 灰度 + Otsu 阈值
    cv::Mat gray = frame.clone();
    if (gray.channels() == 3 || gray.channels() == 4)
        cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);

    cv::Mat binary = gray.clone();
    cv::threshold(binary, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    cv::QRCodeDetector detector;
    std::string data = detector.detectAndDecode(binary);

    if (!data.empty())
    {
        decodedData.assign(reinterpret_cast<const uint8_t*>(data.data()),
                          reinterpret_cast<const uint8_t*>(data.data()) + data.size());
        return true;
    }

    return false;
}

// 帧数据解析
bool QrVideoDecoder::ParseFrameData(const std::vector<uint8_t>& qrData, FrameData& frameData)
{
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

