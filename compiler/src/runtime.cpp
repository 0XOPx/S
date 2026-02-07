#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

using namespace std;

enum Op {
    OP_PUSH_INT,
    OP_LOAD,
    OP_STORE,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    OP_JMP,
    OP_JMP_IF_FALSE,
    OP_CALL,
    OP_RET,
    OP_PRINT,
    OP_PRINT_STR,
    OP_POP
};

struct Function {
    string name;
    int numParams = 0;
    int numLocals = 0;
    vector<int> code;
};

struct Frame {
    int funcIndex;
    int ip;
    vector<int> locals;
};

static const char kMagic[8] = {'S','B','C','0','M','A','G','0'};
static const uint32_t kVersion = 1;

string getSelfPath(char** argv) {
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) return string(buf, len);
    if (argv && argv[0]) return string(argv[0]);
    return string();
}

uint32_t readU32(const vector<uint8_t>& data, size_t& pos) {
    if (pos + 4 > data.size()) throw runtime_error("Unexpected end of payload");
    uint32_t v = 0;
    v |= static_cast<uint32_t>(data[pos]);
    v |= static_cast<uint32_t>(data[pos + 1]) << 8;
    v |= static_cast<uint32_t>(data[pos + 2]) << 16;
    v |= static_cast<uint32_t>(data[pos + 3]) << 24;
    pos += 4;
    return v;
}

string readString(const vector<uint8_t>& data, size_t& pos) {
    uint32_t len = readU32(data, pos);
    if (pos + len > data.size()) throw runtime_error("Unexpected end of payload");
    string s(reinterpret_cast<const char*>(&data[pos]), reinterpret_cast<const char*>(&data[pos]) + len);
    pos += len;
    return s;
}

int runVM(const vector<Function>& functions, const vector<string>& strings, int entryFunc) {
    vector<int> stack;
    vector<Frame> callStack;

    int funcIndex = entryFunc;
    int ip = 0;
    vector<int> locals(functions[funcIndex].numLocals, 0);

    auto pop = [&]() {
        int v = stack.back();
        stack.pop_back();
        return v;
    };

    while (true) {
        const vector<int>& code = functions[funcIndex].code;
        if (ip >= static_cast<int>(code.size())) {
            throw runtime_error("Instruction pointer out of range");
        }
        int op = code[ip++];
        switch (op) {
            case OP_PUSH_INT: stack.push_back(code[ip++]); break;
            case OP_LOAD: stack.push_back(locals.at(code[ip++])); break;
            case OP_STORE: {
                int idx = code[ip++];
                locals.at(idx) = pop();
                break;
            }
            case OP_ADD: { int b = pop(), a = pop(); stack.push_back(a + b); break; }
            case OP_SUB: { int b = pop(), a = pop(); stack.push_back(a - b); break; }
            case OP_MUL: { int b = pop(), a = pop(); stack.push_back(a * b); break; }
            case OP_DIV: {
                int b = pop(), a = pop();
                if (b == 0) throw runtime_error("Division by zero");
                stack.push_back(a / b);
                break;
            }
            case OP_EQ: { int b = pop(), a = pop(); stack.push_back(a == b ? 1 : 0); break; }
            case OP_NE: { int b = pop(), a = pop(); stack.push_back(a != b ? 1 : 0); break; }
            case OP_LT: { int b = pop(), a = pop(); stack.push_back(a < b ? 1 : 0); break; }
            case OP_LE: { int b = pop(), a = pop(); stack.push_back(a <= b ? 1 : 0); break; }
            case OP_GT: { int b = pop(), a = pop(); stack.push_back(a > b ? 1 : 0); break; }
            case OP_GE: { int b = pop(), a = pop(); stack.push_back(a >= b ? 1 : 0); break; }
            case OP_JMP: ip = code[ip]; break;
            case OP_JMP_IF_FALSE: {
                int target = code[ip++];
                int cond = pop();
                if (cond == 0) ip = target;
                break;
            }
            case OP_CALL: {
                int callee = code[ip++];
                int argCount = code[ip++];
                const Function& fn = functions[callee];
                if (argCount != fn.numParams) throw runtime_error("Call arity mismatch");

                vector<int> newLocals(fn.numLocals, 0);
                for (int i = argCount - 1; i >= 0; --i) {
                    newLocals[i] = pop();
                }

                callStack.push_back({funcIndex, ip, locals});
                funcIndex = callee;
                ip = 0;
                locals.swap(newLocals);
                break;
            }
            case OP_RET: {
                int ret = pop();
                if (callStack.empty()) return ret;
                Frame fr = callStack.back();
                callStack.pop_back();
                funcIndex = fr.funcIndex;
                ip = fr.ip;
                locals.swap(fr.locals);
                stack.push_back(ret);
                break;
            }
            case OP_PRINT: {
                int v = pop();
                cout << v << "\n";
                break;
            }
            case OP_PRINT_STR: {
                int idx = code[ip++];
                if (idx < 0 || idx >= static_cast<int>(strings.size())) {
                    throw runtime_error("String index out of range");
                }
                cout << strings[idx] << "\n";
                break;
            }
            case OP_POP: {
                pop();
                break;
            }
            default:
                throw runtime_error("Unknown opcode");
        }
    }
}

int main(int argc, char** argv) {
    try {
        string path = getSelfPath(argv);
        if (path.empty()) throw runtime_error("Cannot determine exe path");

        ifstream in(path, ios::binary);
        if (!in) throw runtime_error("Failed to open self exe");

        in.seekg(0, ios::end);
        int64_t size = static_cast<int64_t>(in.tellg());
        if (size < 12) throw runtime_error("Exe too small");

        in.seekg(size - 12, ios::beg);
        uint32_t payloadSize = 0;
        in.read(reinterpret_cast<char*>(&payloadSize), sizeof(uint32_t));
        char magic[8] = {0};
        in.read(magic, sizeof(magic));
        if (memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
            throw runtime_error("Missing S payload");
        }
        if (payloadSize > static_cast<uint32_t>(size - 12)) {
            throw runtime_error("Invalid payload size");
        }

        int64_t payloadStart = size - 12 - payloadSize;
        in.seekg(payloadStart, ios::beg);
        vector<uint8_t> payload(payloadSize);
        if (payloadSize > 0) in.read(reinterpret_cast<char*>(payload.data()), payloadSize);

        size_t pos = 0;
        uint32_t version = readU32(payload, pos);
        if (version != kVersion) throw runtime_error("Unsupported payload version");

        uint32_t entry = readU32(payload, pos);

        uint32_t numStrings = readU32(payload, pos);
        vector<string> strings;
        strings.reserve(numStrings);
        for (uint32_t i = 0; i < numStrings; ++i) {
            strings.push_back(readString(payload, pos));
        }

        uint32_t numFunctions = readU32(payload, pos);
        vector<Function> functions;
        functions.reserve(numFunctions);
        for (uint32_t i = 0; i < numFunctions; ++i) {
            Function fn;
            fn.name = readString(payload, pos);
            fn.numParams = static_cast<int>(readU32(payload, pos));
            fn.numLocals = static_cast<int>(readU32(payload, pos));
            uint32_t codeLen = readU32(payload, pos);
            fn.code.reserve(codeLen);
            for (uint32_t j = 0; j < codeLen; ++j) {
                fn.code.push_back(static_cast<int>(readU32(payload, pos)));
            }
            functions.push_back(std::move(fn));
        }

        if (entry >= functions.size()) throw runtime_error("Invalid entry function");
        return runVM(functions, strings, static_cast<int>(entry));
    } catch (const exception& ex) {
        cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
