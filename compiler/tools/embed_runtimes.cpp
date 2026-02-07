#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

vector<uint8_t> readAll(const string& path) {
    ifstream in(path, ios::binary);
    if (!in) throw runtime_error("Failed to open " + path);
    return vector<uint8_t>(istreambuf_iterator<char>(in), istreambuf_iterator<char>());
}

void writeHeader(const string& outPath, const string& sym, const vector<uint8_t>& data) {
    ofstream out(outPath, ios::binary);
    if (!out) throw runtime_error("Failed to write " + outPath);
    out << "#pragma once\n#include <cstddef>\n#include <cstdint>\n\n";
    out << "const uint8_t " << sym << "[] = {";
    for (size_t i = 0; i < data.size(); ++i) {
        if (i % 16 == 0) out << "\n    ";
        out << static_cast<unsigned int>(data[i]);
        if (i + 1 < data.size()) out << ", ";
    }
    out << "\n};\n";
    out << "const size_t " << sym << "_size = " << data.size() << ";\n";
}

int main(int argc, char** argv) {
    try {
        if (argc != 5) {
            cerr << "Usage: embed_runtimes <runtime_x64.exe> <runtime_x86.exe> <out_x64.h> <out_x86.h>\n";
            return 1;
        }
        auto x64 = readAll(argv[1]);
        auto x86 = readAll(argv[2]);
        writeHeader(argv[3], "embedded_runtime_x64", x64);
        writeHeader(argv[4], "embedded_runtime_x86", x86);
        return 0;
    } catch (const exception& ex) {
        cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
