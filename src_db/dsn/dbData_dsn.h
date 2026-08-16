#pragma once
#include "../dbDataBase.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace db {
namespace dsn {

// --- 内核子结构（Qt-free） ---

struct Shape {
    enum class Type { Polygon, Circle };
    Type type = Type::Polygon;
    // polygon: 相对焊盘中心的顶点（内存坐标，未应用 transform）
    std::vector<Point2D> polyPts;
    // circle: 相对中心 + 半径
    Point2D circleCenter;
    double circleRadius = 0.0;
    double rotation = 0.0;
    int layer = 1;
};

struct Pad {
    std::vector<Shape> shapes;
};

struct Component {
    Point2D origin;
    std::string direction;
    double rotation = 0.0;
};

struct Net {
    std::vector<std::string> pinNames;
    std::string viaName = "via0";
    double width = 1.0;
    double clearance = 0.4;
};

struct Wiring {
    enum class Type { Wire, Via };
    Type type = Type::Wire;
    Point2D p1, p2;
    int layer = 1;
    double width = 0.0;
    std::string viaName;
    std::string netName;
    // 原 type/protect 字段保留到 extra：
    std::string extraType;
};

// --- 顶层 DSN 内核数据 ---
class Data_dsn : public db::DataBase {
public:
    void clear() override {
        db::DataBase::clear();
        m_pcbInfo.clear();
        m_resolution = 100.0;
        m_viaDefs.clear();
        m_grids.clear();
        m_rules.clear();
        m_defaultWidth = 1.0;
        m_layers.clear();
        m_components.clear();
        m_pads.clear();
        m_pins.clear();
        m_pinPads.clear();
        m_obsPads.clear();
        m_nets.clear();
        m_wirings.clear();
    }

    // 元信息
    std::unordered_map<std::string, std::string> m_pcbInfo;
    double m_resolution = 100.0;  // mil

    // 结构
    std::vector<std::string> m_viaDefs;                             // via 名集合（解析顺序保留）
    std::unordered_map<std::string, double> m_grids;                // place / wire / via
    std::unordered_map<std::string, double> m_rules;                // default / default_smd / smd_smd
    double m_defaultWidth = 1.0;
    std::unordered_map<int, std::string> m_layers;                  // layerId -> name

    // 版图
    std::unordered_map<std::string, Component> m_components;
    std::unordered_map<std::string, Pad> m_pads;                    // padName -> Pad
    std::unordered_map<std::string, Point2D> m_pins;                // pinName -> pin center (绝对坐标)
    std::unordered_map<std::string, std::string> m_pinPads;         // pinName -> padName
    std::unordered_map<std::string, std::string> m_obsPads;         // 不在 net 中的 pin -> pad (在 setPaintData 时计算)

    // 网络
    std::unordered_map<std::string, Net> m_nets;                    // netName -> Net

    // 已有布线
    std::vector<Wiring> m_wirings;

    // --- 单位转换（原 transform()）：mil -> mm (或其他)，直接乘 scale ---
    // scale = 解析得到的 mil->mm 系数 (通常 0.254 * user_unit)
    void applyScale(double scale) {
        m_scale = scale;
        if (std::fabs(scale - 1.0) < 1e-12) return;

        // boundary: x,y 都乘（从下标 1 起）
        if (m_boundary.size() > 1) {
            for (size_t i = 1; i < m_boundary.size(); ++i) m_boundary[i] *= scale;
        }
        // grid/rule
        for (auto& kv : m_grids) kv.second *= scale;
        for (auto& kv : m_rules) kv.second *= scale;
        m_defaultWidth *= scale;

        // component origin
        for (auto& kv : m_components) {
            kv.second.origin.x *= scale;
            kv.second.origin.y *= scale;
        }
        // pad shapes
        for (auto& kv : m_pads) {
            for (auto& s : kv.second.shapes) {
                if (s.type == Shape::Type::Polygon) {
                    for (auto& p : s.polyPts) { p.x *= scale; p.y *= scale; }
                } else {
                    s.circleCenter.x *= scale;
                    s.circleCenter.y *= scale;
                    s.circleRadius *= scale;
                }
            }
        }
        // pins
        for (auto& kv : m_pins) {
            kv.second.x *= scale;
            kv.second.y *= scale;
        }
        // net
        for (auto& kv : m_nets) {
            kv.second.width *= scale;
            kv.second.clearance *= scale;
        }
        // wirings
        for (auto& w : m_wirings) {
            w.p1.x *= scale; w.p1.y *= scale;
            w.p2.x *= scale; w.p2.y *= scale;
            w.width *= scale;
        }
        // bbox
        if (m_bbox.valid()) {
            m_bbox.minPt.x *= scale; m_bbox.minPt.y *= scale;
            m_bbox.maxPt.x *= scale; m_bbox.maxPt.y *= scale;
        }
    }
};

}  // namespace dsn
}  // namespace db
