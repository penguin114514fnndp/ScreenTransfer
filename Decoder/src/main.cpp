#include <iostream>
#include "decode.h"

int main(int argc, char *argv[])
{
    // min_qr_decode <input_video> <output_bin> <output_valid>
    if (argc <= 3)
    {
        std::cout << "Usage: min_qr_decode <input_video> <output_bin> <output_valid>\n"
                  << "Example: min_qr_decode output/qr_video.mp4 output/out.bin output/wout.bin\n";
        return 1;
    }

    std::string inputVideo = argv[1];
    std::string outputBin = argv[2];
    std::string outputValid = argv[3];

    QrVideoDecoder decoder;
    if (!decoder.Init(inputVideo, outputBin, outputValid))
        return 2;

    return decoder.Decode();
}
