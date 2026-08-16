#pragma once
#include <string>
#include <vector>
#include <cctype>
#include <sstream>

namespace db {

inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

inline bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() &&
           s.compare(0, prefix.size(), prefix) == 0;
}

inline std::vector<std::string> splitWhitespace(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) {
        out.push_back(tok);
    }
    return out;
}

// 从字符串中提取所有 double 数字（支持科学计数法、正负号）
inline void extractAllNumbers(std::vector<double>& out, const std::string& s) {
    out.clear();
    size_t i = 0, n = s.size();
    while (i < n) {
        // 跳非数字字符
        while (i < n) {
            char c = s[i];
            if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+') {
                // 数字起点的判定：数字本身；或 +/- 后面紧接数字/点；或 '.' 后面紧接数字
                bool isNumStart = false;
                if (c >= '0' && c <= '9') isNumStart = true;
                else if (c == '-' || c == '+') {
                    if (i + 1 < n) {
                        char nc = s[i + 1];
                        if ((nc >= '0' && nc <= '9') || nc == '.') isNumStart = true;
                    }
                } else if (c == '.') {
                    if (i + 1 < n && s[i + 1] >= '0' && s[i + 1] <= '9') isNumStart = true;
                }
                if (isNumStart) break;
            }
            ++i;
        }
        if (i >= n) break;

        size_t start = i;
        bool sawDot = false, sawExp = false;
        while (i < n) {
            char c = s[i];
            if (c >= '0' && c <= '9') { ++i; continue; }
            if (c == '.' && !sawDot && !sawExp) { sawDot = true; ++i; continue; }
            if ((c == 'e' || c == 'E') && !sawExp) {
                sawExp = true;
                ++i;
                if (i < n && (s[i] == '+' || s[i] == '-')) ++i;
                continue;
            }
            break;
        }
        if (i > start) {
            std::string tok = s.substr(start, i - start);
            try {
                double v = std::stod(tok);
                out.push_back(v);
            } catch (...) {
                // 忽略解析失败
            }
        }
    }
}

}  // namespace db
