#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

static string getExeDir(char** argv) {
    filesystem::path p = filesystem::path(argv[0]);
    if (p.has_parent_path()) return p.parent_path().string();
    return filesystem::current_path().string();
}

static void writeTempProgram(const string& path, const string& line) {
    ofstream out(path, ios::binary);
    if (!out) throw runtime_error("Failed to write temp file");
    out << "int main() {\n";
    out << line << "\n";
    out << "return 0;\n";
    out << "}\n";
}

int main(int argc, char** argv) {
    try {
        string exeDir = getExeDir(argv);
        string compiler = (filesystem::path(exeDir) / "compiler" / "scc.exe").string();

        cout << "S REPL - type :quit to exit\n";
        cout << "Note: one line = one statement (end with ';' if needed)\n";

        string line;
        while (true) {
            cout << "s> ";
            if (!std::getline(cin, line)) break;
            if (line == ":quit" || line == ":q") break;
            if (line.empty()) continue;

            string tmp = (filesystem::path(exeDir) / "__repl_tmp.s").string();
            writeTempProgram(tmp, line);

            string cmd = "\"" + compiler + "\" --run \"" + tmp + "\"";
            int rc = system(cmd.c_str());
            if (rc != 0) {
                cout << "(error)\n";
            }
        }
        return 0;
    } catch (const exception& ex) {
        cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
