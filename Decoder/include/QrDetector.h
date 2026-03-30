#pragma once

#include <vector>
#include <cstdint>
#include <opencv2/opencv.hpp>

class QrDetector
{
public:
    static std::vector<uint8_t> Detect(const cv::Mat& frame);

private:
    static bool DetectWithPyzbar(const cv::Mat& frame, std::vector<uint8_t>& data);
    static bool DetectWithOpenCV(const cv::Mat& frame, std::vector<uint8_t>& data);
};
