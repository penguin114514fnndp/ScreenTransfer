#include "QrDetector.h"
#include <cstdio>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

std::vector<uint8_t> QrDetector::Detect(const cv::Mat& frame)
{
    std::vector<uint8_t> data;

    // 优先使用pyzbar
    if (DetectWithPyzbar(frame, data))
        return data;

    // 备选OpenCV
    if (DetectWithOpenCV(frame, data))
        return data;

    // 都失败，返回空
    return std::vector<uint8_t>();
}

bool QrDetector::DetectWithPyzbar(const cv::Mat& frame, std::vector<uint8_t>& data)
{
    try
    {
        fs::create_directories("output/temp_qr");
        fs::path framePath = "output/temp_qr/temp.png";

        if (!cv::imwrite(framePath.string(), frame))
            return false;

        // 调用Python脚本
        std::string command = "python \"tools/decode_qr.py\" \"" + framePath.string() + "\"";
        FILE* pipe = _popen(command.c_str(), "r");
        if (!pipe)
            return false;

        std::string output;
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
            output += buffer;

        int exitCode = _pclose(pipe);

        // 清理临时文件
        try { fs::remove(framePath); } catch (...) {}

        if (exitCode != 0)
            return false;

        // 解析JSON: {"success": true, "data": "48656c..."}
        size_t pos = output.find("\"data\":");
        if (pos == std::string::npos)
            return false;

        size_t q1 = output.find("\"", pos + 7);
        size_t q2 = output.find("\"", q1 + 1);
        if (q1 == std::string::npos || q2 == std::string::npos)
            return false;

        std::string hexData = output.substr(q1 + 1, q2 - q1 - 1);
        if (hexData == "null" || hexData.empty())
            return false;

        // 十六进制转二进制
        data.clear();
        for (size_t i = 0; i < hexData.length(); i += 2)
        {
            if (i + 1 < hexData.length())
            {
                uint8_t byte = (uint8_t)strtol(hexData.substr(i, 2).c_str(), nullptr, 16);
                data.push_back(byte);
            }
        }

        return !data.empty();
    }
    catch (...)
    {
        return false;
    }
}

bool QrDetector::DetectWithOpenCV(const cv::Mat& frame, std::vector<uint8_t>& data)
{
    try
    {
        // 转灰度
        cv::Mat gray = frame.clone();
        if (gray.channels() == 3 || gray.channels() == 4)
            cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);

        // Otsu二值化
        cv::Mat binary;
        cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

        // QR检测
        cv::QRCodeDetector detector;
        std::string qrData = detector.detectAndDecode(binary);

        if (!qrData.empty())
        {
            data.assign((uint8_t*)qrData.data(), (uint8_t*)qrData.data() + qrData.size());
            return true;
        }

        return false;
    }
    catch (...)
    {
        return false;
    }
}
