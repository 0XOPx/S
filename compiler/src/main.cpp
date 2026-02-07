#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "embedded_runtime_x64.h"
#include "embedded_runtime_x86.h"
#ifdef _WIN32
#include <windows.h>
#endif
using namespace std;

enum class TokType {
    End,
    Ident,
    Number,
    String,
    LParen, RParen,
    LBrace, RBrace,
    Comma, Semicolon,
    Plus, Minus, Star, Slash,
    Assign,
    Eq, Ne, Lt, Le, Gt, Ge,
    KwInt, KwReturn, KwIf, KwElse, KwWhile
};

struct Token {
    TokType type;
    string text;
    int value = 0;
    int line = 1;
    int col = 1;
};

class Lexer {
public:
    explicit Lexer(const string& src) : src_(src) {}

    Token next() {
        skipWhitespaceAndComments();
        if (pos_ >= src_.size()) return makeToken(TokType::End, "");

        char c = src_[pos_];
        if (isalpha(static_cast<unsigned char>(c)) || c == '_') {
            return identOrKeyword();
        }
        if (isdigit(static_cast<unsigned char>(c))) {
            return number();
        }
        if (c == '"') {
            return stringLiteral();
        }

        switch (c) {
            case '(': return simple(TokType::LParen, "(");
            case ')': return simple(TokType::RParen, ")");
            case '{': return simple(TokType::LBrace, "{");
            case '}': return simple(TokType::RBrace, "}");
            case ',': return simple(TokType::Comma, ",");
            case ';': return simple(TokType::Semicolon, ";");
            case '+': return simple(TokType::Plus, "+");
            case '-': return simple(TokType::Minus, "-");
            case '*': return simple(TokType::Star, "*");
            case '/': return simple(TokType::Slash, "/");
            case '=': return match('=') ? simple(TokType::Eq, "==") : simple(TokType::Assign, "=");
            case '!': return match('=') ? simple(TokType::Ne, "!=") : errorToken("Unexpected '!'");
            case '<': return match('=') ? simple(TokType::Le, "<=") : simple(TokType::Lt, "<");
            case '>': return match('=') ? simple(TokType::Ge, ">=") : simple(TokType::Gt, ">");
            default:
                return errorToken(string("Unexpected '") + c + "'");
        }
    }

private:
    Token makeToken(TokType type, const string& text) {
        Token t;
        t.type = type;
        t.text = text;
        t.line = line_;
        t.col = col_;
        return t;
    }

    Token simple(TokType type, const string& text) {
        Token t = makeToken(type, text);
        advance(text.size());
        return t;
    }

    Token errorToken(const string& msg) {
        Token t = makeToken(TokType::End, "");
        t.text = msg;
        return t;
    }

    void advance(size_t n) {
        for (size_t i = 0; i < n; ++i) {
            if (src_[pos_] == '\n') { line_++; col_ = 1; }
            else { col_++; }
            pos_++;
        }
    }

    bool match(char expected) {
        if (pos_ + 1 >= src_.size()) return false;
        return src_[pos_ + 1] == expected;
    }

    void skipWhitespaceAndComments() {
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (isspace(static_cast<unsigned char>(c))) {
                advance(1);
                continue;
            }
            if (c == '/' && pos_ + 1 < src_.size()) {
                if (src_[pos_ + 1] == '/') {
                    while (pos_ < src_.size() && src_[pos_] != '\n') advance(1);
                    continue;
                }
                if (src_[pos_ + 1] == '*') {
                    advance(2);
                    while (pos_ + 1 < src_.size()) {
                        if (src_[pos_] == '*' && src_[pos_ + 1] == '/') {
                            advance(2);
                            break;
                        }
                        advance(1);
                    }
                    continue;
                }
            }
            return;
        }
    }

    Token identOrKeyword() {
        size_t start = pos_;
        int startCol = col_;
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (!isalnum(static_cast<unsigned char>(c)) && c != '_') break;
            advance(1);
        }
        string text = src_.substr(start, pos_ - start);
        Token t;
        t.text = text;
        t.line = line_;
        t.col = startCol;

        if (text == "int") t.type = TokType::KwInt;
        else if (text == "return") t.type = TokType::KwReturn;
        else if (text == "if") t.type = TokType::KwIf;
        else if (text == "else") t.type = TokType::KwElse;
        else if (text == "while") t.type = TokType::KwWhile;
        else t.type = TokType::Ident;
        return t;
    }

    Token number() {
        size_t start = pos_;
        int startCol = col_;
        while (pos_ < src_.size() && isdigit(static_cast<unsigned char>(src_[pos_]))) advance(1);
        string text = src_.substr(start, pos_ - start);
        Token t;
        t.type = TokType::Number;
        t.text = text;
        t.value = stoi(text);
        t.line = line_;
        t.col = startCol;
        return t;
    }

    Token stringLiteral() {
        int startCol = col_;
        advance(1); // opening quote
        string value;
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (c == '"') {
                advance(1);
                Token t;
                t.type = TokType::String;
                t.text = value;
                t.line = line_;
                t.col = startCol;
                return t;
            }
            if (c == '\\') {
                if (pos_ + 1 >= src_.size()) break;
                char n = src_[pos_ + 1];
                switch (n) {
                    case 'n': value.push_back('\n'); break;
                    case 't': value.push_back('\t'); break;
                    case 'r': value.push_back('\r'); break;
                    case '"': value.push_back('"'); break;
                    case '\\': value.push_back('\\'); break;
                    default: value.push_back(n); break;
                }
                advance(2);
                continue;
            }
            if (c == '\n') break;
            value.push_back(c);
            advance(1);
        }
        return errorToken("Unterminated string literal");
    }

    string src_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
};

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

struct Program {
    vector<Function> functions;
    vector<string> strings;
};

struct PendingCall {
    int funcIndex;
    int codePos;
    string name;
    int argCount;
};

class Parser {
public:
    explicit Parser(const string& src) : lexer_(src) {
        tok_ = lexer_.next();
        nextTok_ = lexer_.next();
    }

    Program compile() {
        while (tok_.type != TokType::End) {
            parseFunction();
        }
        resolveCalls();
        Program p;
        p.functions = functions_;
        p.strings = strings_;
        return p;
    }

private:
    void error(const string& msg) {
        ostringstream oss;
        oss << "Parse error at " << tok_.line << ":" << tok_.col << ": " << msg;
        throw runtime_error(oss.str());
    }

    void advance() {
        tok_ = nextTok_;
        nextTok_ = lexer_.next();
        if (tok_.type == TokType::End && !tok_.text.empty()) {
            throw runtime_error(tok_.text);
        }
    }

    bool match(TokType type) {
        if (tok_.type != type) return false;
        advance();
        return true;
    }

    void expect(TokType type, const string& what) {
        if (tok_.type != type) error("Expected " + what);
        advance();
    }

    void parseFunction() {
        expect(TokType::KwInt, "'int'");
        if (tok_.type != TokType::Ident) error("Expected function name");
        string name = tok_.text;
        advance();
        expect(TokType::LParen, "'('");
        vector<string> params;
        if (tok_.type != TokType::RParen) {
            while (true) {
                expect(TokType::KwInt, "'int'");
                if (tok_.type != TokType::Ident) error("Expected parameter name");
                params.push_back(tok_.text);
                advance();
                if (match(TokType::Comma)) continue;
                break;
            }
        }
        expect(TokType::RParen, "')'");

        int funcIndex = addFunction(name);
        currentFunc_ = funcIndex;
        locals_.clear();

        Function& fn = functions_[funcIndex];
        fn.numParams = static_cast<int>(params.size());
        fn.numLocals = fn.numParams;

        for (int i = 0; i < static_cast<int>(params.size()); ++i) {
            locals_[params[i]] = i;
        }

        parseBlock();

        // Ensure function ends with return
        emit(OP_PUSH_INT, 0);
        emit(OP_RET);

        currentFunc_ = -1;
    }

    void parseBlock() {
        expect(TokType::LBrace, "'{'");
        while (tok_.type != TokType::RBrace) {
            parseStatement();
        }
        expect(TokType::RBrace, "'}'");
    }

    void parseStatement() {
        if (tok_.type == TokType::KwInt) {
            parseDeclaration();
            return;
        }
        if (tok_.type == TokType::KwReturn) {
            advance();
            parseExpression();
            expect(TokType::Semicolon, "';'");
            emit(OP_RET);
            return;
        }
        if (tok_.type == TokType::KwIf) {
            parseIf();
            return;
        }
        if (tok_.type == TokType::KwWhile) {
            parseWhile();
            return;
        }
        if (tok_.type == TokType::LBrace) {
            parseBlock();
            return;
        }

        if (tok_.type == TokType::Ident && nextTok_.type == TokType::Assign) {
            string name = tok_.text;
            advance();
            advance();
            parseExpression();
            expect(TokType::Semicolon, "';'");
            int idx = localIndex(name);
            emit(OP_STORE, idx);
            return;
        }

        parseExpression();
        expect(TokType::Semicolon, "';'");
        emit(OP_POP);
    }

    void parseDeclaration() {
        expect(TokType::KwInt, "'int'");
        if (tok_.type != TokType::Ident) error("Expected variable name");
        string name = tok_.text;
        advance();
        int idx = addLocal(name);
        if (match(TokType::Assign)) {
            parseExpression();
            emit(OP_STORE, idx);
        }
        expect(TokType::Semicolon, "';'");
    }

    void parseIf() {
        expect(TokType::KwIf, "'if'");
        expect(TokType::LParen, "'('");
        parseExpression();
        expect(TokType::RParen, "')'");

        int jmpFalsePos = emit(OP_JMP_IF_FALSE, 0);
        parseStatement();

        if (match(TokType::KwElse)) {
            int jmpEndPos = emit(OP_JMP, 0);
            patch(jmpFalsePos, currentCodeSize());
            parseStatement();
            patch(jmpEndPos, currentCodeSize());
        } else {
            patch(jmpFalsePos, currentCodeSize());
        }
    }

    void parseWhile() {
        expect(TokType::KwWhile, "'while'");
        int loopStart = currentCodeSize();
        expect(TokType::LParen, "'('");
        parseExpression();
        expect(TokType::RParen, "')'");
        int jmpFalsePos = emit(OP_JMP_IF_FALSE, 0);
        parseStatement();
        emit(OP_JMP, loopStart);
        patch(jmpFalsePos, currentCodeSize());
    }

    void parseExpression() {
        parseEquality();
    }

    void parseEquality() {
        parseRelational();
        while (tok_.type == TokType::Eq || tok_.type == TokType::Ne) {
            TokType op = tok_.type;
            advance();
            parseRelational();
            emit(op == TokType::Eq ? OP_EQ : OP_NE);
        }
    }

    void parseRelational() {
        parseAdditive();
        while (tok_.type == TokType::Lt || tok_.type == TokType::Le ||
               tok_.type == TokType::Gt || tok_.type == TokType::Ge) {
            TokType op = tok_.type;
            advance();
            parseAdditive();
            switch (op) {
                case TokType::Lt: emit(OP_LT); break;
                case TokType::Le: emit(OP_LE); break;
                case TokType::Gt: emit(OP_GT); break;
                case TokType::Ge: emit(OP_GE); break;
                default: break;
            }
        }
    }

    void parseAdditive() {
        parseTerm();
        while (tok_.type == TokType::Plus || tok_.type == TokType::Minus) {
            TokType op = tok_.type;
            advance();
            parseTerm();
            emit(op == TokType::Plus ? OP_ADD : OP_SUB);
        }
    }

    void parseTerm() {
        parseUnary();
        while (tok_.type == TokType::Star || tok_.type == TokType::Slash) {
            TokType op = tok_.type;
            advance();
            parseUnary();
            emit(op == TokType::Star ? OP_MUL : OP_DIV);
        }
    }

    void parseUnary() {
        if (tok_.type == TokType::Minus) {
            advance();
            parseUnary();
            emit(OP_PUSH_INT, -1);
            emit(OP_MUL);
            return;
        }
        parsePrimary();
    }

    void parsePrimary() {
        if (tok_.type == TokType::Number) {
            emit(OP_PUSH_INT, tok_.value);
            advance();
            return;
        }
        if (tok_.type == TokType::Ident) {
            string name = tok_.text;
            if (nextTok_.type == TokType::LParen) {
                parseCall();
                return;
            }
            advance();
            int idx = localIndex(name);
            emit(OP_LOAD, idx);
            return;
        }
        if (match(TokType::LParen)) {
            parseExpression();
            expect(TokType::RParen, "')'");
            return;
        }
        error("Expected expression");
    }

    void parseCall() {
        if (tok_.type != TokType::Ident) error("Expected function name");
        string name = tok_.text;
        advance();
        expect(TokType::LParen, "'('");
        if (name == "print") {
            if (tok_.type == TokType::String) {
                int idx = addString(tok_.text);
                advance();
                expect(TokType::RParen, "')'");
                emit(OP_PRINT_STR, idx);
                emit(OP_PUSH_INT, 0);
                return;
            }
            if (tok_.type == TokType::RParen) error("print expects 1 argument");
            parseExpression();
            expect(TokType::RParen, "')'");
            emit(OP_PRINT);
            emit(OP_PUSH_INT, 0);
            return;
        }

        int argCount = 0;
        if (tok_.type != TokType::RParen) {
            while (true) {
                if (tok_.type == TokType::String) {
                    error("String literals are only allowed in print(...)");
                }
                parseExpression();
                argCount++;
                if (match(TokType::Comma)) continue;
                break;
            }
        }
        expect(TokType::RParen, "')'");

        int callPos = emit(OP_CALL, 0);
        emit(argCount);
        pendingCalls_.push_back({currentFunc_, callPos, name, argCount});
    }

    int addFunction(const string& name) {
        auto it = funcIndex_.find(name);
        if (it != funcIndex_.end()) {
            // Duplicate definition
            error("Function already defined: " + name);
        }
        int idx = static_cast<int>(functions_.size());
        funcIndex_[name] = idx;
        functions_.push_back(Function());
        functions_[idx].name = name;
        return idx;
    }

    int addLocal(const string& name) {
        if (locals_.count(name)) error("Variable already defined: " + name);
        Function& fn = functions_[currentFunc_];
        int idx = fn.numLocals;
        fn.numLocals++;
        locals_[name] = idx;
        return idx;
    }

    int addString(const string& s) {
        auto it = stringIndex_.find(s);
        if (it != stringIndex_.end()) return it->second;
        int idx = static_cast<int>(strings_.size());
        strings_.push_back(s);
        stringIndex_[s] = idx;
        return idx;
    }

    int localIndex(const string& name) {
        auto it = locals_.find(name);
        if (it == locals_.end()) error("Unknown variable: " + name);
        return it->second;
    }

    int emit(int op) {
        Function& fn = functions_[currentFunc_];
        fn.code.push_back(op);
        return static_cast<int>(fn.code.size()) - 1;
    }

    int emit(int op, int operand) {
        Function& fn = functions_[currentFunc_];
        fn.code.push_back(op);
        fn.code.push_back(operand);
        return static_cast<int>(fn.code.size()) - 1;
    }

    void patch(int pos, int target) {
        Function& fn = functions_[currentFunc_];
        fn.code[pos] = target;
    }

    int currentCodeSize() {
        return static_cast<int>(functions_[currentFunc_].code.size());
    }

    void resolveCalls() {
        for (const auto& call : pendingCalls_) {
            auto it = funcIndex_.find(call.name);
            if (it == funcIndex_.end()) {
                throw runtime_error("Unknown function: " + call.name);
            }
            int idx = it->second;
            if (functions_[idx].numParams != call.argCount) {
                throw runtime_error("Function " + call.name + " expects " +
                                    to_string(functions_[idx].numParams) + " args, got " +
                                    to_string(call.argCount));
            }
            functions_[call.funcIndex].code[call.codePos] = idx;
        }
    }

    Lexer lexer_;
    Token tok_;
    Token nextTok_;
    vector<Function> functions_;
    unordered_map<string, int> funcIndex_;
    unordered_map<string, int> locals_;
    unordered_map<string, int> stringIndex_;
    vector<PendingCall> pendingCalls_;
    vector<string> strings_;
    int currentFunc_ = -1;
};

struct Frame {
    int funcIndex;
    int ip;
    vector<int> locals;
};

int runVM(const Program& program, int entryFunc) {
    const vector<Function>& functions = program.functions;
    const vector<string>& strings = program.strings;
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
            throw runtime_error("Instruction pointer out of range in function " + functions[funcIndex].name);
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

static const uint32_t kVersion = 1;

void appendU32(vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

void appendString(vector<uint8_t>& out, const string& s) {
    appendU32(out, static_cast<uint32_t>(s.size()));
    out.insert(out.end(), s.begin(), s.end());
}

vector<uint8_t> buildPayload(const Program& program, int entryFunc) {
    vector<uint8_t> out;
    appendU32(out, kVersion);
    appendU32(out, static_cast<uint32_t>(entryFunc));

    appendU32(out, static_cast<uint32_t>(program.strings.size()));
    for (const auto& s : program.strings) {
        appendString(out, s);
    }

    appendU32(out, static_cast<uint32_t>(program.functions.size()));
    for (const auto& fn : program.functions) {
        appendString(out, fn.name);
        appendU32(out, static_cast<uint32_t>(fn.numParams));
        appendU32(out, static_cast<uint32_t>(fn.numLocals));
        appendU32(out, static_cast<uint32_t>(fn.code.size()));
        for (int v : fn.code) {
            appendU32(out, static_cast<uint32_t>(v));
        }
    }
    return out;
}

vector<uint8_t> readFileBytes(const string& path) {
    ifstream in(path, ios::binary);
    if (!in) throw runtime_error("Failed to open " + path);
    return vector<uint8_t>(istreambuf_iterator<char>(in), istreambuf_iterator<char>());
}

void writeExeWithPayload(const vector<uint8_t>& base, const string& outExe, const vector<uint8_t>& payload) {
    ofstream out(outExe, ios::binary);
    if (!out) throw runtime_error("Failed to create " + outExe);

    if (!base.empty()) out.write(reinterpret_cast<const char*>(base.data()), base.size());
#ifdef _WIN32
    out.close();
    if (!out) throw runtime_error("Failed to write " + outExe);
    HANDLE h = BeginUpdateResourceA(outExe.c_str(), FALSE);
    if (!h) throw runtime_error("BeginUpdateResource failed");
    WORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    if (!UpdateResourceA(h, RT_RCDATA, MAKEINTRESOURCEA(101), lang,
                         (void*)payload.data(), static_cast<DWORD>(payload.size()))) {
        EndUpdateResourceA(h, TRUE);
        throw runtime_error("UpdateResource failed");
    }
    if (!EndUpdateResourceA(h, FALSE)) {
        throw runtime_error("EndUpdateResource failed");
    }
#else
    (void)payload;
    throw runtime_error("Resource embedding is only supported on Windows");
#endif
}

vector<uint8_t> runtimeBytesForArch(const string& arch) {
    if (arch == "x86") {
        return vector<uint8_t>(embedded_runtime_x86, embedded_runtime_x86 + embedded_runtime_x86_size);
    }
    if (arch == "x64") {
        return vector<uint8_t>(embedded_runtime_x64, embedded_runtime_x64 + embedded_runtime_x64_size);
    }
    throw runtime_error("Unknown arch: " + arch);
}

string detectSystemArch() {
#ifdef _WIN32
    SYSTEM_INFO info;
    GetNativeSystemInfo(&info);
    switch (info.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: return "x64";
        case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
        case PROCESSOR_ARCHITECTURE_ARM64: return "x64"; // no ARM64 runtime; fall back to x64
        default: return "x64";
    }
#else
    return "x64";
#endif
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            cerr << "Usage: scc <file.s> -o <out.exe> --arch x64\n";
            cerr << "   or: scc <file.s> -o <out.exe>--arch x64\n";
            cerr << "   or: scc --run <file.s>\n";
            return 1;
        }

        bool runMode = false;
        bool archExplicit = false;
#if defined(_WIN64)
        string arch = "x64";
#else
        string arch = "x86";
#endif
        int argi = 1;
        if (string(argv[argi]) == "--run") {
            runMode = true;
            argi++;
        }
        if (argi < argc && string(argv[argi]) == "--arch") {
            if (argi + 1 >= argc) throw runtime_error("Expected --arch x64|x86");
            arch = argv[argi + 1];
            archExplicit = true;
            argi += 2;
        }
        if (argi >= argc) throw runtime_error("Missing input file");

        string inputPath = argv[argi++];
        string outExe;

        if (!runMode) {
            if (argi + 1 >= argc || string(argv[argi]) != "-o") {
                throw runtime_error("Usage: scc <file.s> -o <out.exe> [--arch x64|x86]");
            }
            outExe = argv[argi + 1];
        }

        ifstream in(inputPath);
        if (!in) {
            cerr << "Failed to open " << inputPath << "\n";
            return 1;
        }

        string src((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());

        Parser parser(src);
        Program program = parser.compile();

        auto it = find_if(program.functions.begin(), program.functions.end(), [](const Function& f) {
            return f.name == "main";
        });
        if (it == program.functions.end()) {
            throw runtime_error("No main function found");
        }
        if (it->numParams != 0) {
            throw runtime_error("main must take 0 parameters");
        }
        int entry = static_cast<int>(it - program.functions.begin());

        if (runMode) {
            return runVM(program, entry);
        }

        if (!archExplicit) {
            string detected = detectSystemArch();
            cout << "--arch not there, get the PC's architecture\n";
            cout << "Found architecture to be \"" << detected << "\" running " << detected << " runtime\n";
            arch = detected;
        } else {
            cout << "Using --arch \"" << arch << "\" runtime\n";
        }

        vector<uint8_t> base = runtimeBytesForArch(arch);
        if (base.empty()) {
            throw runtime_error("Embedded runtime is empty. Rebuild embedded runtimes.");
        }
        vector<uint8_t> payload = buildPayload(program, entry);
        writeExeWithPayload(base, outExe, payload);
        return 0;

    } catch (const exception& ex) {
        cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}

