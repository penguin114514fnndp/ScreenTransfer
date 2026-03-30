#include <iostream>
#include <fstream>
#include <vector>
#include <opencv2/opencv.hpp>

using namespace std;

int main()
{
    cout << "=== OpenCV QR Detection Test ===" << endl;
    cout << "OpenCV version: " << CV_VERSION << endl;
    
    // 测试1: 尝试加载PPM
    cout << "\n=== Loading PPM ===" << endl;
    cv::Mat ppm = cv::imread("output/frames/frame_0.ppm");
    
    if (ppm.empty()) {
        cerr << "Failed to load PPM!" << endl;
        return 1;
    }
    
    cout << "PPM size: " << ppm.rows << "x" << ppm.cols << endl;
    cout << "Channels: " << ppm.channels() << endl;
    cout << "Depth: " << ppm.depth() << endl;
    
    // 测试2: 尝试QR检测
    cout << "\n=== QR Detection Test ===" << endl;
    cv::QRCodeDetector detector;
    
    // 直接在PPM上检测
    cout << "Attempting detection on original PPM..." << endl;
    string data = detector.detectAndDecode(ppm);
    cout << "Result: " << (data.empty() ? "FAILED" : "SUCCESS - " + to_string(data.length()) + " bytes") << endl;
    
    if (!data.empty()) {
        // 打印前16个字节
        cout << "Data (first 16 bytes): ";
        for (int i = 0; i < min(16, (int)data.length()); i++) {
            printf("%02x ", (unsigned char)data[i]);
        }
        cout << endl;
        return 0;
    }
    
    // 测试3: 转灰度再试
    cout << "\nTrying with grayscale..." << endl;
    cv::Mat gray;
    cv::cvtColor(ppm, gray, cv::COLOR_BGR2GRAY);
    data = detector.detectAndDecode(gray);
    cout << "Result: " << (data.empty() ? "FAILED" : "SUCCESS") << endl;
    
    // 测试4: 二值化
    cout << "\nTrying with Otsu binary..." << endl;
    cv::Mat binary;
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    data = detector.detectAndDecode(binary);
    cout << "Result: " << (data.empty() ? "FAILED" : "SUCCESS") << endl;
    
    // 测试5: 反向
    cout << "\nTrying with inverted binary..." << endl;
    cv::Mat inverted;
    cv::bitwise_not(binary, inverted);
    data = detector.detectAndDecode(inverted);
    cout << "Result: " << (data.empty() ? "FAILED" : "SUCCESS") << endl;
    
    cout << "\n✗ All strategies failed to detect QR" << endl;
    return 1;
}
