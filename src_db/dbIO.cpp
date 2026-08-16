#include "dbIO.h"
#include "dbString.h"
#include <fstream>

namespace db {

bool readLines(const std::string& path,
               const std::function<void(std::string& line)>& callback,
               bool skipSharpComments) {
    std::ifstream fin(path);
    if (!fin.is_open()) return false;

    std::string line;
    while (std::getline(fin, line)) {
        // 剥离行末 \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string t = trim(line);
        if (t.empty()) continue;
        if (skipSharpComments && t[0] == '#') continue;
        callback(line);
    }
    return true;
}

bool readAllLines(const std::string& path,
                  std::vector<std::string>& outLines,
                  bool skipSharpComments) {
    outLines.clear();
    return readLines(path, [&](std::string& l) { outLines.push_back(l); }, skipSharpComments);
}

}  // namespace db
