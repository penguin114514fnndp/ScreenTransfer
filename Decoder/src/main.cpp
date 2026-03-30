#include <iostream>
#include <opencv2/opencv.hpp>
#include <cstdio>
#include "QrVideoDecoder.h"

int testPPMDetection(const std::string& ppmFile)
{
    std::cout << "=== Direct PPM QR Detection Test ===" << std::endl;
    std::cout << "Testing: " << ppmFile << std::endl;

    // 首先尝试pyzbar (已验证有效!)
    std::cout << "\n1. Testing with pyzbar:" << std::endl;
    std::string command = "python decode_qr_pyzbar.py \"" + ppmFile + "\" 2>nul";
    FILE* pipe = _popen(command.c_str(), "r");
    if (pipe)
    {
        std::string output;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            output += buffer;
        }
        _pclose(pipe);

        if (output.find("\"success\": true") != std::string::npos)
        {
            std::cout << "   [SUCCESS] Pyzbar detected QR code!" << std::endl;
            return 0;
        }
    }
    std::cout << "   [FAILED] Pyzbar unavailable" << std::endl;

    // 如果pyzbar失败，尝试OpenCV (备选方案)
    std::cout << "\n2. Testing with OpenCV (fallback):" << std::endl;

    // 强制读取为灰度！这是关键修复
    cv::Mat img = cv::imread(ppmFile, cv::IMREAD_GRAYSCALE);
    if (img.empty())
    {
        std::cerr << "Failed to load PPM: " << ppmFile << std::endl;
        return 1;
    }

    std::cout << "Image size: " << img.rows << "x" << img.cols << std::endl;
    std::cout << "Channels: " << img.channels() << " (forced to grayscale)" << std::endl;

    cv::QRCodeDetector detector;
    std::string data;

    // 测试1: 原始灰度
    std::cout << "\nTest 1 (Grayscale PPM): ";
    data = detector.detectAndDecode(img);
    if (!data.empty()) {
        std::cout << "✓ SUCCESS! (" << data.size() << " bytes)" << std::endl;
        std::cout << "First 20 bytes (hex): ";
        for (size_t i = 0; i < std::min(size_t(20), data.size()); i++)
            printf("%02x ", (unsigned char)data[i]);
        std::cout << std::endl;
        return 0;
    }
    std::cout << "FAILED" << std::endl;

    // 测试2: Otsu二值化
    cv::Mat binary;
    cv::threshold(img, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    std::cout << "Test 2 (Otsu Binary): ";
    data = detector.detectAndDecode(binary);
    if (!data.empty()) {
        std::cout << "✓ SUCCESS!" << std::endl;
        return 0;
    }
    std::cout << "FAILED" << std::endl;

    // 测试3: 反向
    cv::Mat inverted;
    cv::bitwise_not(binary, inverted);

    std::cout << "Test 3 (Inverted): ";
    data = detector.detectAndDecode(inverted);
    if (!data.empty()) {
        std::cout << "✓ SUCCESS!" << std::endl;
        return 0;
    }
    std::cout << "FAILED" << std::endl;

    // 测试4: 自适应阈值
    cv::Mat adaptive;
    cv::adaptiveThreshold(img, adaptive, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                         cv::THRESH_BINARY, 11, 2);

    std::cout << "Test 4 (Adaptive): ";
    data = detector.detectAndDecode(adaptive);
    if (!data.empty()) {
        std::cout << "✓ SUCCESS!" << std::endl;
        return 0;
    }
    std::cout << "FAILED" << std::endl;

    std::cout << "\n✗ PPM QR Detection: ALL STRATEGIES FAILED" << std::endl;
    return 2;
}

int main(int argc, char *argv[])
{
    // 检查是否是PPM测试模式
    if (argc == 3 && std::string(argv[1]) == "decode-ppm")
    {
        return testPPMDetection(argv[2]);
    }

    // 正常的视频解码模式
    if (argc != 4 && argc != 5)
    {
        std::cout << "Usage: decoder <input_video> <output_bin> <output_validity> [reference_bin]\n"
                  << "       decoder decode-ppm <input_ppm>  (test PPM detection)\n"
                  << "Example: decoder recorded.mp4 out.bin vout.bin\n"
                  << "Example: decoder decode-ppm output/frames/frame_0.ppm\n";
        return 1;
    }

    std::string inputVideo = argv[1];
    std::string outputBin = argv[2];
    std::string outputValidity = argv[3];
    std::string referenceBin = (argc == 5) ? argv[4] : "";

    QrVideoDecoder decoder;
    if (!decoder.Init(inputVideo, outputBin, outputValidity, referenceBin))
        return 1;

    const int decodeStatus = decoder.Decode();
    decoder.DisplayDecodingReport();
    return decodeStatus;
}