#pragma once
#include "dbPoint.h"
#include <vector>

namespace db {

// 内核数据基类：仅保存"读取的原始数据"。不含绘图样式、不含前端显示边距 margin。
// 任何文件格式（DSN/DEF/LEF 等）的内核数据都继承此类。
class DataBase {
public:
    virtual ~DataBase() = default;
    virtual void clear() {
        m_boundary.clear();
        m_bbox = Box2D();
        m_scale = 1.0;
    }

    // 边界数据：多边形边界 = [0(layer占位), x1, y1, x2, y2, ...]
    // （保留原有"下标0起按 x,y 交替"的解析逻辑顺序，便于前端复用）
    std::vector<double> m_boundary;

    // 内核边界 Box：解析时 computeBBox 计算
    Box2D m_bbox;

    // 内核单位系数（0.254 = mil→mm，解析 resolution 后设置）
    double m_scale = 1.0;
};

}  // namespace db
