#pragma once
#include "../dbParserBase.h"
#include "dbData_dsn.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace db {
namespace dsn {

class Parser_dsn : public db::ParserBase {
public:
    Parser_dsn() : m_data(nullptr) {}
    explicit Parser_dsn(Data_dsn* data) : m_data(data) {}
    void bind(Data_dsn* data) { m_data = data; }
    bool readFile(const std::string& path) override;

protected:
    void computeBBox() override;

private:
    Data_dsn* m_data = nullptr;
    std::unordered_map<std::string, int> m_layerMap;  // layerName -> layerId

    // 主调度
    void parseBlock(const std::vector<std::string>& lines, int type);

    // 模块解析
    void parseParser   (const std::vector<std::string>& lines);
    void parseResolution(const std::vector<std::string>& lines);
    void parseStructure (const std::vector<std::string>& lines);
    void parsePlacement (const std::vector<std::string>& lines);
    void parseLibrary   (const std::vector<std::string>& lines);
    void parseNetwork   (const std::vector<std::string>& lines);
    void parseWiring    (const std::vector<std::string>& lines);

    // 子解析器（structure）
    void parseBoundary (const std::vector<std::string>& lines, size_t& i);
    void parseVia      (const std::vector<std::string>& lines, size_t& i);
    void parseGrid     (const std::vector<std::string>& lines, size_t& i);
    void parseRule     (const std::vector<std::string>& lines, size_t& i);
    void parseLayer    (const std::vector<std::string>& lines, size_t& i);

    // 子解析器（library）
    void parseLibPin  (const std::vector<std::string>& lines, size_t& i);
    void parseLibPad  (const std::vector<std::string>& lines, size_t& i);

    // 子解析器（network）
    void parseNet         (const std::vector<std::string>& lines, size_t& i);
    void parseNetClass    (const std::vector<std::string>& lines, size_t& i);
    void parseClassCircuit(const std::vector<std::string>& lines, const std::string& netName, size_t& i, int& bracketCount);
    void parseClassRule   (const std::vector<std::string>& lines, const std::string& netName, size_t& i, int& bracketCount);

    // 子解析器（wiring）
    void parseWire(const std::vector<std::string>& lines, size_t& i);
    void parseViaWire(const std::vector<std::string>& lines, size_t& i);

    // 工具
    static int bracketCount(const std::string& line);
};

}  // namespace dsn
}  // namespace db
