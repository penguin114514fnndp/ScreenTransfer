#include <iostream>
#include "QrVideoDecoder.h"

int main(int argc, char *argv[])
{
    // decode <input_video> <output_bin> <output_validity> [reference_bin]
    if (argc != 4 && argc != 5)
    {
        std::cout << "Usage: decode <input_video> <output_bin> <output_validity> [reference_bin]\n"
                  << "Example: decode recorded.mp4 out.bin vout.bin\n"
                  << "Example: decode recorded.mp4 out.bin vout.bin source.bin\n";
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