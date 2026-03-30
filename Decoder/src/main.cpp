#include <iostream>
#include "QrVideoDecoder.h"
#include "DataVerifier.h"

int main(int argc, char *argv[])
{
    if (argc < 3 || argc > 4)
    {
        std::cout << "Usage: decoder <input.mp4> <output.bin> [reference.bin]\n";
        return 1;
    }

    std::cout << "\n=== QR VIDEO DECODER ===\n";

    // 解码video
    QrVideoDecoder decoder;
    if (!decoder.Init(argv[1], argv[2]))
        return 1;

    int status = decoder.Decode();
    if (status != 0)
        return status;

    // 如果提供了参考文件，进行比对
    if (argc == 4)
    {
        DataVerifier verifier;
        verifier.Compare(argv[2], argv[3]);
    }
}
