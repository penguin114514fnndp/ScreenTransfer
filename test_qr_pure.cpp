#include <iostream>
#include <fstream>
#include <vector>
#include "Encoder/include/qrcodegen.hpp"

using namespace std;
using qrcodegen::QrCode;

void savePPM(const QrCode& qr, const string& filename, int scale) {
    ofstream ofs(filename, ios::binary);
    const int size = qr.getSize() * scale;
    ofs << "P5\n" << size << " " << size << "\n255\n";
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            ofs.put(qr.getModule(x / scale, y / scale) ? 255 : 0);
        }
    }
}

int main() {
    // Test with PURE data (no frame header)
    cout << "Generating QR from pure test_input.bin (no frame header)..." << endl;
    
    ifstream ifs("output/test_input.bin", ios::binary);
    vector<uint8_t> data((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    
    cout << "Data size: " << data.size() << " bytes" << endl;
    
    try {
        QrCode qr = QrCode::encodeBinary(data, QrCode::Ecc::LOW);
        cout << "QR Version: " << qr.getVersion() << endl;
        cout << "QR Size: " << qr.getSize() << "x" << qr.getSize() << endl;
        
        savePPM(qr, "output/frame_pure.ppm", 20);
        cout << "✓ Saved to output/frame_pure.ppm" << endl;
    } catch (const exception& e) {
        cout << "Error: " << e.what() << endl;
    }
    
    return 0;
}
