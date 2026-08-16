#include "dbParser_dsn.h"
#include "../dbIO.h"
#include "../dbString.h"
#include <regex>
#include <sstream>

namespace db {
namespace dsn {

// 工具
static int bracketCountLine(const std::string& line) {
    int c = 0;
    for (char ch : line) {
        if (ch == '(') ++c;
        else if (ch == ')') --c;
    }
    return c;
}

int Parser_dsn::bracketCount(const std::string& line) {
    return bracketCountLine(line);
}

void Parser_dsn::computeBBox() {
    if (!m_data) return;
    auto& b = m_data->m_boundary;
    Box2D box;
    if (b.size() > 2) {
        // boundary: [0]=layer(占位), [1]=x1,[2]=y1,[3]=x2,[4]=y2 ...
        for (size_t i = 1; i + 1 < b.size(); i += 2) {
            box.expand(Point2D(b[i], b[i + 1]));
        }
    }
    // pins
    for (const auto& kv : m_data->m_pins) box.expand(kv.second);
    // wiring
    for (const auto& w : m_data->m_wirings) {
        box.expand(w.p1);
        box.expand(w.p2);
    }
    m_data->m_bbox = box;
}

// ============ 主调度 ============
bool Parser_dsn::readFile(const std::string& path) {
    if (!m_data) return false;
    m_data->clear();
    m_layerMap.clear();

    std::vector<std::string> lines;
    if (!readAllLines(path, lines, false)) return false;

    std::vector<std::string> block;
    int type = -1;
    for (auto& rawLine : lines) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        if (line[0] == '#' || (line.size() >= 2 && line[0] == '/' && line[1] == '/')) continue;

        if (startsWith(line, "(PCB")) {
            parseBlock(block, type);
            type = 1;
            block.clear();
        }
        else if (startsWith(line, "(parser")) {
            parseBlock(block, type);
            type = 1;
            block.clear();
        }
        else if (startsWith(line, "(resolution")) {
            parseBlock(block, type);
            type = 2;
            block.clear();
        }
        else if (startsWith(line, "(structure")) {
            parseBlock(block, type);
            type = 3;
            block.clear();
        }
        else if (startsWith(line, "(placement")) {
            parseBlock(block, type);
            type = 4;
            block.clear();
        }
        else if (startsWith(line, "(library")) {
            parseBlock(block, type);
            type = 5;
            block.clear();
        }
        else if (startsWith(line, "(network")) {
            parseBlock(block, type);
            type = 6;
            block.clear();
        }
        else if (startsWith(line, "(wiring")) {
            parseBlock(block, type);
            type = 7;
            block.clear();
        }
        block.push_back(line);
    }
    parseBlock(block, type);

    computeBBox();
    return true;
}

void Parser_dsn::parseBlock(const std::vector<std::string>& lines, int type) {
    switch (type) {
        case -1: break;
        case 1: parseParser(lines);    break;
        case 2: parseResolution(lines); break;
        case 3: parseStructure(lines); break;
        case 4: parsePlacement(lines); break;
        case 5: parseLibrary(lines);   break;
        case 6: parseNetwork(lines);   break;
        case 7: parseWiring(lines);    break;
        default: break;
    }
}

// ============ 1. Parser (PCB/parser) ============
void Parser_dsn::parseParser(const std::vector<std::string>& lines) {
    // "(xxx\"yyy\")"
    std::regex re(R"(^\((\w+)\s*\"([^\"]+)\"\)?)");
    std::smatch m;
    for (const auto& line : lines) {
        if (std::regex_search(line, m, re)) {
            m_data->m_pcbInfo[m[1].str()] = m[2].str();
        }
    }
}

// ============ 2. Resolution ============
void Parser_dsn::parseResolution(const std::vector<std::string>& lines) {
    std::regex re(R"(^\(resolution\s+mil\s+(\d+\.?\d*)\s*\)?)");
    std::smatch m;
    for (const auto& line : lines) {
        if (std::regex_search(line, m, re)) {
            try {
                m_data->m_resolution = std::stod(m[1].str());
            } catch (...) {}
            break;
        }
    }
}

// ============ 3. Structure ============
void Parser_dsn::parseStructure(const std::vector<std::string>& lines) {
    size_t n = lines.size();
    for (size_t i = 1; i < n; ++i) {
        const auto& line = lines[i];
        if (startsWith(line, "(boundary")) parseBoundary(lines, i);
        else if (startsWith(line, "(via")) parseVia(lines, i);
        else if (startsWith(line, "(grid")) parseGrid(lines, i);
        else if (startsWith(line, "(rule(")) parseRule(lines, i);
        else if (startsWith(line, "(layer")) parseLayer(lines, i);
    }
}

void Parser_dsn::parseBoundary(const std::vector<std::string>& lines, size_t& i) {
    (void)i;
    std::vector<double> nums;
    extractAllNumbers(nums, lines[i]);
    for (double v : nums) m_data->m_boundary.push_back(v);
}

void Parser_dsn::parseVia(const std::vector<std::string>& lines, size_t& i) {
    (void)i;
    // "(via via0 via1 ...)"
    std::regex re(R"(^\(via\s+(.+?)\s*\)?$)");
    std::smatch m;
    if (std::regex_search(lines[i], m, re)) {
        auto words = splitWhitespace(m[1].str());
        for (const auto& w : words) {
            // 去重
            bool dup = false;
            for (const auto& v : m_data->m_viaDefs) if (v == w) { dup = true; break; }
            if (!dup) m_data->m_viaDefs.push_back(w);
        }
    }
}

void Parser_dsn::parseGrid(const std::vector<std::string>& lines, size_t& i) {
    (void)i;
    std::regex re(R"(^\(grid\s+(\w+)\s+(\d+\.?\d*)\)?)");
    std::smatch m;
    if (std::regex_search(lines[i], m, re)) {
        try {
            m_data->m_grids[m[1].str()] = std::stod(m[2].str());
        } catch (...) {}
    }
}

void Parser_dsn::parseRule(const std::vector<std::string>& lines, size_t& i) {
    (void)i;
    // "(rule(clear 0.4))"  "(rule(clear 0.4(type smd)))"  "(rule(width 1))"
    std::regex re(R"(^\(rule\(\s*(\w+)\s+(\d+\.?\d*)\s*(?:\(type\s+(\w+)\))?\)*)");
    std::smatch m;
    if (std::regex_search(lines[i], m, re)) {
        std::string ruleName = m[1].str();
        try {
            double value = std::stod(m[2].str());
            if (ruleName == "clear") {
                std::string clearType = "default";
                if (m.size() >= 4 && m[3].matched) clearType = m[3].str();
                m_data->m_rules[clearType] = value;
            } else if (ruleName == "width") {
                m_data->m_defaultWidth = value;
            }
        } catch (...) {}
    }
}

void Parser_dsn::parseLayer(const std::vector<std::string>& lines, size_t& i) {
    std::string layerName;
    std::regex reName(R"(^\(layer\s+([^\s\)]+))");
    std::smatch m;
    if (!std::regex_search(lines[i], m, reName)) return;
    layerName = m[1].str();
    int bracketCount = 1;

    while (bracketCount > 0 && i + 1 < lines.size()) {
        const auto& line = lines[++i];
        if (startsWith(line, "(type")) {
            bracketCount += bracketCountLine(line);
            std::regex typeRe(R"(^\(type\s+([^\s\)]+))");
            std::smatch tm;
            if (std::regex_search(line, tm, typeRe)) {
                auto it = m_layerMap.find(layerName);
                int layerId;
                if (it == m_layerMap.end()) {
                    layerId = (int)m_layerMap.size() + 1;
                    m_layerMap[layerName] = layerId;
                } else {
                    layerId = it->second;
                }
                m_data->m_layers[layerId] = tm[1].str();
            }
        }
        else if (startsWith(line, ")")) {
            --bracketCount;
        }
        else if (startsWith(line, "(layer")) {
            // 下一个 layer，回退一行给外层
            --i;
            break;
        }
        else {
            bracketCount += bracketCountLine(line);
        }
    }
}

// ============ 4. Placement ============
void Parser_dsn::parsePlacement(const std::vector<std::string>& lines) {
    size_t n = lines.size();
    std::regex compRe(R"(^\(component\s+(\w+))");
    std::regex placeRe(R"(^\(place\s+(\w+)\s+([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\s+([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\s+(\w+)\s+([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\)?)");
    for (size_t i = 1; i < n; ++i) {
        const auto& line = lines[i];
        if (startsWith(line, "(component")) {
            std::smatch m;
            std::string compName = "u1";
            if (std::regex_search(line, m, compRe)) {
                compName = m[1].str();
                m_data->m_components[compName] = Component();
            }
            if (i + 1 >= n) break;
            const auto& next = lines[++i];
            if (startsWith(next, "(place")) {
                std::smatch pm;
                if (std::regex_search(next, pm, placeRe)) {
                    std::string realName = pm[1].str();
                    Component c;
                    try {
                        c.origin.x = std::stod(pm[2].str());
                        c.origin.y = std::stod(pm[3].str());
                        c.direction = pm[4].str();
                        c.rotation  = std::stod(pm[5].str());
                    } catch (...) {}
                    m_data->m_components[realName] = c;
                }
            } else if (startsWith(next, "(component")) {
                --i;
            }
        }
    }
}

// ============ 5. Library ============
void Parser_dsn::parseLibrary(const std::vector<std::string>& lines) {
    size_t n = lines.size();
    for (size_t i = 1; i < n; ++i) {
        const auto& line = lines[i];
        if (startsWith(line, "(image")) parseLibPin(lines, i);
        else if (startsWith(line, "(padstack")) parseLibPad(lines, i);
    }
}

void Parser_dsn::parseLibPin(const std::vector<std::string>& lines, size_t& i) {
    std::regex imageRe(R"(^\(image\s+(\w+))");
    std::regex pinRe(R"(^\(pin\s+(\w+)\s+(\w+)\s+([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\s+([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\)?)");
    std::smatch m;
    if (!std::regex_search(lines[i], m, imageRe)) return;
    (void)m;  // imageName 不保存（padstack 另解析）
    int bracketCount = 1;

    while (bracketCount > 0 && i + 1 < lines.size()) {
        const auto& line = lines[++i];
        if (startsWith(line, "(pin")) {
            bracketCount += bracketCountLine(line);
            std::smatch pm;
            if (std::regex_search(line, pm, pinRe)) {
                std::string pdName  = pm[1].str();
                std::string pinName = pm[2].str();
                Pad& pad = m_data->m_pads[pdName];
                (void)pad;
                m_data->m_pinPads[pinName] = pdName;
                try {
                    double x = std::stod(pm[3].str());
                    double y = std::stod(pm[4].str());
                    m_data->m_pins[pinName] = Point2D(x, y);
                } catch (...) {}
            }
        }
        else if (startsWith(line, ")")) {
            --bracketCount;
        }
        else {
            bracketCount += bracketCountLine(line);
        }
    }
}

void Parser_dsn::parseLibPad(const std::vector<std::string>& lines, size_t& i) {
    std::regex padRe(R"(^\(padstack\s+([^\s\)]+))");
    std::smatch pm;
    if (!std::regex_search(lines[i], pm, padRe)) return;
    std::string padName = pm[1].str();
    Pad& pad = m_data->m_pads[padName];

    // (shape(circle LAYER ...))
    //  格式1: (shape(polygon SIG_1 0 x1 y1 x2 y2 ...))
    //  格式2: (shape(circle TOP 0 50))  -> 直径=100? 原代码把第一个数字/2做半径
    std::regex shapeRe(R"(^\(shape\(\s*(\w+)\s+([^\s\)]+)\s*(.*)\)\)\s*$)");
    std::regex numRe(R"([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)");

    int bracketCount = 1;
    size_t n = lines.size() - 1;
    while (bracketCount > 0 && i < n) {
        std::string line = trim(lines[++i]);
        if (startsWith(line, "(shape(")) {
            bracketCount += bracketCountLine(line);
            std::smatch sm;
            if (!std::regex_search(line, sm, shapeRe)) continue;
            Shape s;
            std::string typeName = sm[1].str();
            if (typeName == "polygon")      s.type = Shape::Type::Polygon;
            else if (typeName == "circle")  s.type = Shape::Type::Circle;
            else {
                // 不认识的 type，跳过
                continue;
            }
            std::string layerName = sm[2].str();
            auto lit = m_layerMap.find(layerName);
            if (lit == m_layerMap.end()) continue;  // 未知 layer 跳过
            s.layer = lit->second;

            std::string numberPart = sm[3].str();
            std::vector<double> values;
            auto begin = std::sregex_iterator(numberPart.begin(), numberPart.end(), numRe);
            auto end = std::sregex_iterator();
            for (auto it = begin; it != end; ++it) {
                try { values.push_back(std::stod((*it).str())); }
                catch (...) {}
            }
            if (s.type == Shape::Type::Polygon) {
                if (!values.empty()) {
                    s.rotation = values[0];
                    for (size_t k = 1; k + 1 < values.size(); k += 2) {
                        s.polyPts.emplace_back(values[k], values[k + 1]);
                    }
                }
            } else {  // circle
                if (values.size() >= 3) {
                    s.circleCenter.x = values[1];
                    s.circleCenter.y = values[2];
                    s.circleRadius   = values[0] / 2.0;  // 第一个数是直径
                } else if (values.size() >= 1) {
                    s.circleRadius = values[0] / 2.0;
                }
            }
            pad.shapes.push_back(std::move(s));
        }
        else if (startsWith(line, ")")) {
            --bracketCount;
        }
        else {
            bracketCount += bracketCountLine(line);
        }
    }
}

// ============ 6. Network ============
void Parser_dsn::parseNetwork(const std::vector<std::string>& lines) {
    size_t n = lines.size();
    for (size_t i = 1; i < n; ++i) {
        const auto& line = lines[i];
        if (startsWith(line, "(net")) parseNet(lines, i);
        else if (startsWith(line, "(class")) parseNetClass(lines, i);
    }
}

void Parser_dsn::parseNet(const std::vector<std::string>& lines, size_t& i) {
    std::regex netRe(R"(^\(net\s+(\S+))");
    std::regex pinsRe(R"(\b\w+-(\w+)\b)");
    std::smatch m;
    if (!std::regex_search(lines[i], m, netRe)) return;
    std::string netName = m[1].str();
    Net& net = m_data->m_nets[netName];
    int bracketCount = bracketCountLine(lines[i]);

    size_t n1 = lines.size() - 1;
    while (bracketCount > 0 && i < n1) {
        const auto& line = lines[++i];
        if (startsWith(line, "(pins")) {
            bracketCount += bracketCountLine(line);
            auto begin = std::sregex_iterator(line.begin(), line.end(), pinsRe);
            auto end = std::sregex_iterator();
            for (auto it = begin; it != end; ++it) {
                net.pinNames.push_back((*it)[1].str());
            }
        }
        else if (startsWith(line, ")")) {
            --bracketCount;
        }
        else {
            bracketCount += bracketCountLine(line);
        }
    }
}

void Parser_dsn::parseNetClass(const std::vector<std::string>& lines, size_t& i) {
    std::regex re(R"(\(class\s+([^)\s']+))");
    std::smatch m;
    if (!std::regex_search(lines[i], m, re)) return;
    std::string netName = m[1].str();
    (void)m_data->m_nets[netName];  // 确保存在
    int bracketCount = 1;

    size_t n1 = lines.size() - 1;
    do {
        const auto& line = lines[++i];
        if (startsWith(line, "(circuit"))
            parseClassCircuit(lines, netName, i, bracketCount);
        else if (startsWith(line, "(rule"))
            parseClassRule(lines, netName, i, bracketCount);
        else
            bracketCount += bracketCountLine(line);
    } while (bracketCount > 0 && i < n1);
}

void Parser_dsn::parseClassCircuit(const std::vector<std::string>& lines, const std::string& netName, size_t& i, int& bracketCount) {
    --i;
    std::regex re(R"(\((?:\w+)?use_via\s+(\w+)\))");
    size_t n1 = lines.size() - 1;
    do {
        const auto& line = lines[++i];
        bracketCount += bracketCountLine(line);
        std::smatch m;
        if (std::regex_search(line, m, re)) {
            m_data->m_nets[netName].viaName = m[1].str();
        }
    } while (bracketCount > 1 && i < n1);
}

void Parser_dsn::parseClassRule(const std::vector<std::string>& lines, const std::string& netName, size_t& i, int& bracketCount) {
    --i;
    std::regex re(R"(^\((\w+)\s+([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\)?)");
    size_t n1 = lines.size() - 1;
    do {
        const auto& line = lines[++i];
        bracketCount += bracketCountLine(line);
        std::smatch m;
        if (std::regex_search(line, m, re)) {
            std::string ruleName = m[1].str();
            try {
                double v = std::stod(m[2].str());
                if (ruleName == "width") m_data->m_nets[netName].width = v;
                else if (ruleName == "clearance") m_data->m_nets[netName].clearance = v;
            } catch (...) {}
        }
    } while (bracketCount > 1 && i < n1);
}

// ============ 7. Wiring ============
void Parser_dsn::parseWiring(const std::vector<std::string>& lines) {
    size_t n = lines.size();
    for (size_t i = 1; i < n; ++i) {
        const auto& line = lines[i];
        if (startsWith(line, "(wire")) parseWire(lines, i);
        else if (startsWith(line, "(via")) parseViaWire(lines, i);
    }
}

void Parser_dsn::parseWire(const std::vector<std::string>& lines, size_t& i) {
    Wiring w;
    w.type = Wiring::Type::Wire;
    std::regex pathRe(R"(\(path\s+(\d+)\s+([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\s+([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\s+([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\s+([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\s+([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\s*\))");
    std::regex netRe(R"(\(net\s+(\S+)\)$)");
    std::regex typeRe(R"(\(type\s+(\S+)\)$)");
    size_t n = lines.size();
    int bracketCount = 0;
    std::string line = lines[i];
    do {
        line = lines[i];
        bracketCount += bracketCountLine(line);
        if (startsWith(line, "(wire")) {
            // 同行 path？
            std::smatch pm;
            if (line.find("(path") != std::string::npos && std::regex_search(line, pm, pathRe)) {
                try {
                    w.layer = std::stoi(pm[1].str());
                    w.width = std::stod(pm[2].str());
                    w.p1.x = std::stod(pm[3].str());
                    w.p1.y = std::stod(pm[4].str());
                    w.p2.x = std::stod(pm[5].str());
                    w.p2.y = std::stod(pm[6].str());
                } catch (...) {}
            }
        } else if (startsWith(line, "(path")) {
            std::smatch pm;
            if (std::regex_search(line, pm, pathRe)) {
                try {
                    w.layer = std::stoi(pm[1].str());
                    w.width = std::stod(pm[2].str());
                    w.p1.x = std::stod(pm[3].str());
                    w.p1.y = std::stod(pm[4].str());
                    w.p2.x = std::stod(pm[5].str());
                    w.p2.y = std::stod(pm[6].str());
                } catch (...) {}
            }
        } else if (startsWith(line, "(net")) {
            std::smatch nm;
            if (std::regex_search(line, nm, netRe)) w.netName = nm[1].str();
        } else if (startsWith(line, "(type")) {
            std::smatch tm;
            if (std::regex_search(line, tm, typeRe)) w.extraType = tm[1].str();
        }
        ++i;
    } while (bracketCount > 0 && i < n);
    --i;
    if (!w.netName.empty()) m_data->m_wirings.push_back(std::move(w));
}

void Parser_dsn::parseViaWire(const std::vector<std::string>& lines, size_t& i) {
    Wiring w;
    w.type = Wiring::Type::Via;
    std::regex viaRe(R"(\(via\s+(\w+)\s+([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\s+([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?))");
    std::regex netRe(R"(\(net\s+(\w+))");
    std::regex typeRe(R"(\(type\s+(\w+))");
    size_t n = lines.size();
    int bracketCount = 0;
    do {
        std::string line = trim(lines[i]);
        bracketCount += bracketCountLine(line);
        if (startsWith(line, "(via")) {
            std::smatch vm;
            if (std::regex_search(line, vm, viaRe)) {
                w.viaName = vm[1].str();
                try {
                    w.p1.x = std::stod(vm[2].str());
                    w.p1.y = std::stod(vm[3].str());
                    w.p2 = w.p1;
                } catch (...) {}
            }
        } else if (startsWith(line, "(net")) {
            std::smatch nm;
            if (std::regex_search(line, nm, netRe)) w.netName = nm[1].str();
        } else if (startsWith(line, "(type")) {
            std::smatch tm;
            if (std::regex_search(line, tm, typeRe)) w.extraType = tm[1].str();
        }
        ++i;
    } while (bracketCount > 0 && i < n);
    --i;
    if (!w.netName.empty()) m_data->m_wirings.push_back(std::move(w));
}

}  // namespace dsn
}  // namespace db
