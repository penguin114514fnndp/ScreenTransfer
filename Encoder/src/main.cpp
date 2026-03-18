#include <iostream>
#include "QrVideoEncoder.h"

int main(int argc, char *argv[])
{
    // min_qr_video <input_binary_image> <output_video> [duration_ms]
    if (argc <= 3)
    {
        std::cout << "Usage: min_qr_video <input_binary_image> <output_video> [duration_ms]\n"
                  << "Example: min_qr_video demo_input.ppm output/min_qr.mp4 1000\n";
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputVideo = argv[2];
    int durationMs = stoi(argv[3]);

    QrVideoEncoder encoder;
    if (!encoder.Init(inputFile, outputVideo, durationMs))
        return 2;
    encoder.DisplayBandwidthReport();
    return encoder.Encode();
}

// TODO: zlib压缩