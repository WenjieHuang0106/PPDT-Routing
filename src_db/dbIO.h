#pragma once
#include <functional>
#include <string>
#include <vector>

namespace db {

// 逐行读取文本文件，按回调函数处理每一行。
// - 跳过空行（纯空白）
// - 可选择跳过 # 开头的注释行
// - 返回 false 表示文件无法打开
bool readLines(const std::string& path,
               const std::function<void(std::string& line)>& callback,
               bool skipSharpComments = false);

// 读取整个文件到 vector<string>，行末换行符已剥离
bool readAllLines(const std::string& path,
                  std::vector<std::string>& outLines,
                  bool skipSharpComments = false);

}  // namespace db
