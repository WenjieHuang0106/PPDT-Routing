#pragma once
#include <cmath>
#include <algorithm>

namespace db {

struct Point2D {
    double x = 0.0, y = 0.0;
    Point2D() = default;
    Point2D(double a, double b) : x(a), y(b) {}
    bool operator==(const Point2D& o) const {
        return std::fabs(x - o.x) < 1e-9 && std::fabs(y - o.y) < 1e-9;
    }
    bool operator!=(const Point2D& o) const { return !(*this == o); }
    Point2D operator+(const Point2D& o) const { return {x + o.x, y + o.y}; }
    Point2D operator-(const Point2D& o) const { return {x - o.x, y - o.y}; }
    Point2D operator*(double s) const { return {x * s, y * s}; }
    double dot(const Point2D& o) const { return x * o.x + y * o.y; }
    double cross(const Point2D& o) const { return x * o.y - y * o.x; }
    double dist2(const Point2D& o) const {
        double dx = x - o.x, dy = y - o.y;
        return dx * dx + dy * dy;
    }
    double length() const { return std::sqrt(x * x + y * y); }
};

struct Box2D {
    Point2D minPt, maxPt;
    Box2D() : minPt(1e200, 1e200), maxPt(-1e200, -1e200) {}
    bool valid() const { return minPt.x <= maxPt.x && minPt.y <= maxPt.y; }
    void expand(const Point2D& p) {
        minPt.x = std::min(minPt.x, p.x);
        minPt.y = std::min(minPt.y, p.y);
        maxPt.x = std::max(maxPt.x, p.x);
        maxPt.y = std::max(maxPt.y, p.y);
    }
    void expand(double margin) {
        minPt.x -= margin;
        minPt.y -= margin;
        maxPt.x += margin;
        maxPt.y += margin;
    }
    double minX() const { return minPt.x; }
    double minY() const { return minPt.y; }
    double maxX() const { return maxPt.x; }
    double maxY() const { return maxPt.y; }
};

}  // namespace db
