#include <iostream>
#include <opencv2/opencv.hpp>
#include "Encoder/include/QrVideoEncoder.h"
#include "Encoder/include/qrcodegen.hpp"

using namespace std;
using qrcodegen::QrCode;

int main()
{
    // 测试1: 生成最小的QR码
    cout << "=== Test 1: Minimal QR Code ===" << endl;
    try {
        vector<uint8_t> minData = {0x00};  // 1字节数据
        QrCode qr1 = QrCode::encodeBinary(minData, QrCode::Ecc::LOW);
        cout << "QR Size: " << qr1.getSize() << "x" << qr1.getSize() << endl;
        
        // 直接用opencv检测（使用opencv读取PPM）
        // 先找到SaveQrCodeAsPpm的实现...这个是内联的
    } catch (const exception& e) {
        cout << "Error: " << e.what() << endl;
    }

    // 测试2: 创建一个简单的黑白图像测试OpenCV detector
    cout << "\n=== Test 2: Simple B&W Image ===" << endl;
    cv::Mat testImg = cv::Mat(100, 100, CV_8UC1, cv::Scalar(255));  // 白色
    // 画一个黑色正方形
    cv::rectangle(testImg, cv::Point(10, 10), cv::Point(40, 40), cv::Scalar(0), -1);
    
    cv::QRCodeDetector detector;
    string data = detector.detectAndDecode(testImg);
    cout << "Detection result: " << (data.empty() ? "FAILED" : "SUCCESS") << endl;

    // 测试3: 尝试读取已存在的PPM
    cout << "\n=== Test 3: Load PPM and Detect ===" << endl;
    cv::Mat ppm = cv::imread("output/frames/frame_0.ppm", cv::IMREAD_UNCHANGED);
    if (ppm.empty()) {
        cout << "Failed to load PPM" << endl;
        return 1;
    }
    
    cout << "PPM loaded: " << ppm.size() << ", channels: " << ppm.channels() << endl;
    
    // 尝试多种变体检测
    vector<pair<string, cv::Mat>> variants;
    
    // 原始
    variants.push_back({"Raw RGB", ppm.clone()});
    
    // 灰度
    cv::Mat gray;
    if (ppm.channels() == 3) {
        cv::cvtColor(ppm, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = ppm.clone();
    }
    variants.push_back({"Grayscale", gray.clone()});
    
    // 二值化
    cv::Mat binary;
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    variants.push_back({"Binary Otsu", binary.clone()});
    
    // 反向
    cv::Mat inverted;
    cv::bitwise_not(binary, inverted);
    variants.push_back({"Inverted", inverted.clone()});
    
    for (auto& [name, img] : variants) {
        string result = detector.detectAndDecode(img);
        cout << name << ": " << (result.empty() ? "FAILED" : "SUCCESS (" + to_string(result.size()) + " bytes)") << endl;
    }
    
    // 测试4: 尝试较小的版本
    cout << "\n=== Test 4: Downscale and Detect ===" << endl;
    cv::Mat small;
    cv::resize(ppm, small, cv::Size(600, 600), 0, 0, cv::INTER_AREA);
    
    // 转灰度
    if (small.channels() == 3) {
        cv::cvtColor(small, small, cv::COLOR_BGR2GRAY);
    }
    
    string smallResult = detector.detectAndDecode(small);
    cout << "Downscaled (600x600): " << (smallResult.empty() ? "FAILED" : "SUCCESS") << endl;

    return 0;
}
