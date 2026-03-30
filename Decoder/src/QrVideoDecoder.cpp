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
    mTotalFrames = 0;
    mDetectedFrames = 0;

    if (!fs::exists(inputVideo))
    {
        std::cerr << "Error: Input video file not found: " << inputVideo << "\n";
        return false;
    }

    const auto outputDir = outputBin.parent_path();
    if (!outputDir.empty() && !fs::exists(outputDir))
        fs::create_directories(outputDir);

    // 打开诊断日志文件
    fs::path diagDir = outputDir;
    if (diagDir.empty()) diagDir = ".";
    fs::path diagFile = diagDir / "qr_decoder_diagnostics.log";
    mDiagnosticFile.open(diagFile, std::ios::out | std::ios::trunc);
    if (!mDiagnosticFile)
    {
        std::cerr << "Warning: Cannot create diagnostic log file: " << diagFile << "\n";
    }
    else
    {
        LogDiagnostic("=== QR Video Decoder Diagnostics ===");
        LogDiagnostic("Input video: " + inputVideo.string());
    }

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
    mTotalFrames = 0;
    mDetectedFrames = 0;

    while (cap.read(frame) && !frame.empty())
    {
        mTotalFrames++;
        std::vector<uint8_t> qrData;
        FrameData frameData;

        // 识别QR码 解析数据
        std::cout << "Processing frame " << std::setw(5) << std::setfill('0') << mTotalFrames << std::endl;
        if (!RecognizeQrCode(frame, qrData, mTotalFrames) || !ParseFrameData(qrData, frameData))
            continue;
        // 检查重复
        if (mFrames.find(frameData.frameNumber) != mFrames.end())
            continue;

        mDetectedFrames++;
        if (mPayloadSize == 0)
            mPayloadSize = frameData.payload.size();

        maxFrameNumber = std::max(maxFrameNumber, frameData.frameNumber);
        mFrames[frameData.frameNumber] = std::move(frameData);
    }
    cap.release();

    LogDiagnostic("Total frames processed: " + std::to_string(mTotalFrames));
    LogDiagnostic("Frames successfully detected: " + std::to_string(mDetectedFrames));
    if (mTotalFrames > 0)
    {
        double detectionRate = 100.0 * mDetectedFrames / mTotalFrames;
        LogDiagnostic("Detection rate: " + std::to_string(detectionRate) + "%");
    }

    if (mFrames.empty())
    {
        std::cerr << "Error: No valid QR frame recognized in video.\n";
        LogDiagnostic("ERROR: No valid QR frames detected!");
        return 2;
    }

    std::vector<uint8_t> completeData = AssembleCompleteData(maxFrameNumber); // 生成完整数据
    std::vector<uint8_t> validityData = GenerateValidity(completeData);  // 生成有效性标记
    if (!WriteFile(mOutputBin, completeData) || !WriteFile(mOutputValidity, validityData))
    {
        std::cerr << "Error: Failed to write output files.\n";
        LogDiagnostic("ERROR: Failed to write output files!");
        return 3;
    }

    LogDiagnostic("Decoding completed successfully");
    if (mDiagnosticFile.is_open())
        mDiagnosticFile.close();

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
        {
            return false;
        }

        // 调用Python脚本: python decode_qr_pyzbar.py <frame_path>
        std::string command = "python decode_qr_pyzbar.py \"" + framePath.string() + "\" 2>nul";

        // 执行命令并捕获输出
        FILE* pipe = _popen(command.c_str(), "r");
        if (!pipe)
        {
            return false;
        }

        std::string output;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            output += buffer;
        }
        _pclose(pipe);

        // 清理临时文件
        try {
            fs::remove(framePath);
        } catch (...) {}

        // 解析JSON输出 - 简简单方法：查找"data"字段
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
        {
            LogDiagnostic("Frame " + std::to_string(frameIndex) + ": Pyzbar detection - SUCCESS");
            return true;
        }

        return false;
    }
    catch (const std::exception& e)
    {
        return false;
    }
}

bool QrVideoDecoder::RecognizeQrCode(const cv::Mat& frame, std::vector<uint8_t>& decodedData, int frameIndex)
{
    // **首先尝试使用pyzbar (已验证有效)**
    if (DetectQrWithPyzbar(frame, decodedData, frameIndex))
    {
        return true;
    }

    const int kMaxDecodeSide = 3000;  // 增加最大解码尺寸以保留QR细节

    // 调试：保存原始帧
    static bool first_frame = true;
    if (first_frame)
    {
        first_frame = false;
        cv::imwrite("output/debug_frame_raw.png", frame);
        LogDiagnostic("Saved debug frame. Dimensions: " + std::to_string(frame.rows) + "x" + std::to_string(frame.cols) +
                      ", Channels: " + std::to_string(frame.channels()));
    }

    // 如果帧过大，缩放
    cv::Mat toProcess = frame;
    int maxSide = std::max(frame.rows, frame.cols);
    if (maxSide > kMaxDecodeSide)
    {
        double scale = static_cast<double>(kMaxDecodeSide) / maxSide;
        cv::resize(frame, toProcess, cv::Size(), scale, scale, cv::INTER_AREA);
    }

    cv::QRCodeDetector detector;
    std::string data;

    // 策略1: 原始图像（彩色或灰度）
    data = detector.detectAndDecode(toProcess);
    if (!data.empty())
    {
        LogDiagnostic("Frame " + std::to_string(frameIndex) + ": Strategy 1 (Raw frame) - SUCCESS");
        decodedData.assign(reinterpret_cast<const uint8_t*>(data.data()),
                          reinterpret_cast<const uint8_t*>(data.data()) + data.size());
        return true;
    }

    // 策略2: 灰度 + Otsu 阈值
    cv::Mat gray = toProcess.clone();
    if (gray.channels() == 3 || gray.channels() == 4)
        cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);

    cv::Mat binary = gray.clone();
    cv::threshold(binary, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    data = detector.detectAndDecode(binary);
    if (!data.empty())
    {
        LogDiagnostic("Frame " + std::to_string(frameIndex) + ": Strategy 2 (Grayscale + Otsu) - SUCCESS");
        decodedData.assign(reinterpret_cast<const uint8_t*>(data.data()),
                          reinterpret_cast<const uint8_t*>(data.data()) + data.size());
        return true;
    }

    // 策略3: 灰度 + 自适应阈值 (Adaptive threshold)
    cv::Mat adaptive = gray.clone();
    cv::adaptiveThreshold(gray, adaptive, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                         cv::THRESH_BINARY, 11, 2);

    data = detector.detectAndDecode(adaptive);
    if (!data.empty())
    {
        LogDiagnostic("Frame " + std::to_string(frameIndex) + ": Strategy 3 (Adaptive threshold) - SUCCESS");
        decodedData.assign(reinterpret_cast<const uint8_t*>(data.data()),
                          reinterpret_cast<const uint8_t*>(data.data()) + data.size());
        return true;
    }

    // 策略4: 双边滤波 + Otsu 阈值 (降噪)
    cv::Mat denoised = gray.clone();
    cv::bilateralFilter(gray, denoised, 9, 75, 75);
    cv::Mat denoised_binary = denoised.clone();
    cv::threshold(denoised, denoised_binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    data = detector.detectAndDecode(denoised_binary);
    if (!data.empty())
    {
        LogDiagnostic("Frame " + std::to_string(frameIndex) + ": Strategy 4 (Bilateral + Otsu) - SUCCESS");
        decodedData.assign(reinterpret_cast<const uint8_t*>(data.data()),
                          reinterpret_cast<const uint8_t*>(data.data()) + data.size());
        return true;
    }

    // 策略5: 形态学操作 + Otsu 阈值 (填充孔洞)
    cv::Mat morphed = binary.clone();
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(binary, morphed, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 1);

    data = detector.detectAndDecode(morphed);
    if (!data.empty())
    {
        LogDiagnostic("Frame " + std::to_string(frameIndex) + ": Strategy 5 (Morphological close) - SUCCESS");
        decodedData.assign(reinterpret_cast<const uint8_t*>(data.data()),
                          reinterpret_cast<const uint8_t*>(data.data()) + data.size());
        return true;
    }

    // 策略6: 对比度增强 (CLAHE) + Otsu
    cv::Mat clahe_result = gray.clone();
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    clahe->apply(gray, clahe_result);
    cv::Mat clahe_binary = clahe_result.clone();
    cv::threshold(clahe_result, clahe_binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    data = detector.detectAndDecode(clahe_binary);
    if (!data.empty())
    {
        LogDiagnostic("Frame " + std::to_string(frameIndex) + ": Strategy 6 (CLAHE + Otsu) - SUCCESS");
        decodedData.assign(reinterpret_cast<const uint8_t*>(data.data()),
                          reinterpret_cast<const uint8_t*>(data.data()) + data.size());
        return true;
    }

    // 策略7: 上采样 + Otsu (最后一招)
    if (toProcess.rows < 400 || toProcess.cols < 400)
    {
        cv::Mat upscaled;
        cv::resize(toProcess, upscaled, cv::Size(), 2.0, 2.0, cv::INTER_LINEAR);
        cv::Mat gray_up = upscaled.clone();
        if (gray_up.channels() == 3 || gray_up.channels() == 4)
            cv::cvtColor(gray_up, gray_up, cv::COLOR_BGR2GRAY);

        cv::Mat binary_up = gray_up.clone();
        cv::threshold(gray_up, binary_up, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

        data = detector.detectAndDecode(binary_up);
        if (!data.empty())
        {
            LogDiagnostic("Frame " + std::to_string(frameIndex) + ": Strategy 7 (Upscaled + Otsu) - SUCCESS");
            decodedData.assign(reinterpret_cast<const uint8_t*>(data.data()),
                              reinterpret_cast<const uint8_t*>(data.data()) + data.size());
            return true;
        }
    }

    // 策略8: 直方图均衡化 + Otsu
    cv::Mat eq_gray = gray.clone();
    cv::equalizeHist(eq_gray, eq_gray);
    cv::Mat eq_binary = eq_gray.clone();
    cv::threshold(eq_gray, eq_binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    data = detector.detectAndDecode(eq_binary);
    if (!data.empty())
    {
        LogDiagnostic("Frame " + std::to_string(frameIndex) + ": Strategy 8 (Histogram equalization) - SUCCESS");
        decodedData.assign(reinterpret_cast<const uint8_t*>(data.data()),
                          reinterpret_cast<const uint8_t*>(data.data()) + data.size());
        return true;
    }

    // 策略9: 反阈值 + Otsu
    cv::Mat inverted = gray.clone();
    cv::threshold(inverted, inverted, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    data = detector.detectAndDecode(inverted);
    if (!data.empty())
    {
        LogDiagnostic("Frame " + std::to_string(frameIndex) + ": Strategy 9 (Inverted + Otsu) - SUCCESS");
        decodedData.assign(reinterpret_cast<const uint8_t*>(data.data()),
                          reinterpret_cast<const uint8_t*>(data.data()) + data.size());
        return true;
    }

    // 策略10: Canny边界检测 + 膨胀
    cv::Mat canny_img = gray.clone();
    cv::Canny(gray, canny_img, 50, 150);
    cv::Mat kernel_canny = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::dilate(canny_img, canny_img, kernel_canny, cv::Point(-1, -1), 2);

    data = detector.detectAndDecode(canny_img);
    if (!data.empty())
    {
        LogDiagnostic("Frame " + std::to_string(frameIndex) + ": Strategy 10 (Canny + dilate) - SUCCESS");
        decodedData.assign(reinterpret_cast<const uint8_t*>(data.data()),
                          reinterpret_cast<const uint8_t*>(data.data()) + data.size());
        return true;
    }

    LogDiagnostic("Frame " + std::to_string(frameIndex) + ": All 10 strategies FAILED");
    return false;
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

// 诊断日志记录
void QrVideoDecoder::LogDiagnostic(const std::string& message)
{
    if (mDiagnosticFile.is_open())
    {
        mDiagnosticFile << message << "\n";
        mDiagnosticFile.flush();
    }
}
