#pragma once
#include "dbDataBase.h"
#include <string>

namespace db {

// 解析器基类：纯 C++，仅使用 std::string 路径。
// 子类：dbParser_dsn（DSN 格式）、dbParser_def（DEF 格式）、dbParser_lef（LEF 格式）等。
class ParserBase {
public:
    virtual ~ParserBase() = default;

    // 读文件 → 填充子类绑定的 db::DataBase 及其派生结构
    // 返回 false 表示打开失败或解析失败
    virtual bool readFile(const std::string& path) = 0;

    // 可选：写回（默认空实现，返回 true）
    virtual bool writeFile(const std::string& /*path*/) { return true; }

protected:
    // 子类必须实现：根据内核数据计算 bbox 并写入 db::DataBase::m_bbox
    virtual void computeBBox() = 0;
};

}  // namespace db
