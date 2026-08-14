#pragma once
#include <vector>
#include <iostream>

const double MapMinValue = 5e-4;											// 各种单位化后的计算的最小值1e-4
const double PI = 3.14159265358979323846;									// Pi
const double PE = PI / 180.0f;												// 角度转弧度 角度乘PE可得弧度值

const double MaxNumber = 1e20;
const double INF = 1e200;

class Point {
public:
	Point() {
		this->x = 0;
		this->y = 0;
	}

	Point(const double& x, const double& y) {
		this->x = x;
		this->y = y;
	}

	//一般要锁住才调这个 LockPoint = true
	Point(double x, double y, bool LockPoint) {
		this->x = x;
		this->y = y;
		this->locked = LockPoint;
	}


public:

	void setLocked(bool Lock) { this->locked = Lock; }
	bool getLocked() const { return this->locked; }
	double distanceTo(const Point& other) const {
		return sqrt((x - other.x) * (x - other.x) + (y - other.y) * (y - other.y));
	}
	double distanceToLine(const Point& a, const Point& b) const {
		//点到直线距离
		Point ab = b - a;
		Point ap = *this - a;
		double abLen2 = ab * ab;  // 用点积求模的平方，避免开方
		if (abLen2 < minNumber) 
			return distanceTo(a);
		double crossAbs = std::fabs(ab.cross(ap));
		double abLength = std::sqrt(abLen2);
		return crossAbs / abLength;
	}
	double distanceToEdge(const Point& a, const Point& b) const {
		//点到线段距离
		Point ab = b - a;
		Point ap = *this - a;
		double abLen2 = ab.x * ab.x + ab.y * ab.y;
		if (abLen2 < minNumber)
			return distanceTo(a);
		double t = std::max(0.0, std::min(1.0, (ap * ab) / abLen2));
		Point projection = a + ab * t;
		return distanceTo(projection);
	}
	bool ptInMidOfEdge(const Point& a, const Point& b, bool lineWithPt12 = true) const {
		if (!inSameLine(a, b)) {
			return false;
		}
		Point ab = b - a;
		Point ap = *this - a;
		double crossVal = ab.cross(ap);
		if (fabs(crossVal) >= minNumber) {  // 叉积不为0，不在同一直线上
			return false;
		}
		double dotProduct = ap * ab;
		double abLen2 = ab * ab;
		if (abLen2 < minNumber) {
			if (lineWithPt12)
				return distanceTo(a) < minNumber;
			else
				return false;
		}
		double t = dotProduct / abLen2;
		if (lineWithPt12)
			return t >= 0.0 - minNumber && t <= 1.0 + minNumber;
		else
			return t > 0.0 + minNumber && t < 1.0 - minNumber;
	}
	Point& rotate90() {
		if (fabs(x) < minNumber) x = 0;
		if (fabs(y) < minNumber) y = 0;
		double temp = x;
		x = y;
		y = -temp;
		return *this;
	}
	Point shadePointToLine(const Point& a, const Point& b) const {
		Point ab = b - a;
		Point ap = *this - a;
		double abLen2 = ab.x * ab.x + ab.y * ab.y;
		if (abLen2 < 0.0001) return a;

		double t = (ap * ab) / abLen2;
		t = std::max(0.0, std::min(1.0, t));
		return a + ab * t;
	}
	Point normalizeVec()const {    //取单位向量
		double length = sqrt(this->x * this->x + this->y * this->y);
		if (length < minNumber)
			return Point(0, 0);
		else
			return (*this / length);
	}
	double cross(const Point& other) const { //叉积
		return x * other.y - y * other.x;
	}
	bool inSameLine(const Point& p2, const Point& p3) const {
		if (*this == p2 || *this == p3 || p2 == p3)
			return true;
		Point v1 = (p2 - *this).normalizeVec();
		Point v2 = (p3 - *this).normalizeVec();
		return fabs(v1.cross(v2)) < minNumber;
	}
	double vecLength()const {      //取向量的模长
		return sqrt(this->x * this->x + this->y * this->y);
	}

	double operator*(const Point& a) const { return x * a.x + y * a.y; }	//点积
	Point operator*(const double& k) const { return { x * k , y * k }; }

	Point operator/(const double& k) const { return { x / k , y / k }; }
	Point operator+(const Point& a) const { return { x + a.x , y + a.y }; }
	Point& operator+=(const Point& other) { x += other.x; y += other.y; return *this; }
	Point operator-(const Point& other) const {
		return Point(x - other.x, y - other.y);
	}
	Point operator/(const Point& a) const {
		if (abs(a.x) < minNumber || abs(a.y) < minNumber)
			return { 0, 0 };
		return { x / a.x , y / a.y };
	}
	bool operator<(const Point& other) const { return (x < other.x) || (x == other.x && y < other.y); }
	bool operator==(const Point& p) const { return (fabs(x - p.x) < minNumber && fabs(y - p.y) < minNumber); }
	bool operator!=(const Point& p) const { return (fabs(x - p.x) > minNumber || fabs(y - p.y) > minNumber); }

	friend std::ostream& operator<<(std::ostream& print, const Point& p) {
		print << "(" << p.x << "," << p.y << ")";
		return print;
	}
	struct Hash {

		size_t operator()(const Point& p) const {
			// 将坐标量化到误差范围内的同一值
			double quantized_x = std::round(p.x / p.minNumber) * p.minNumber;
			double quantized_y = std::round(p.y / p.minNumber) * p.minNumber;

			size_t h1 = std::hash<double>{}(quantized_x);
			size_t h2 = std::hash<double>{}(quantized_y);
			size_t h3 = std::hash<bool>{}(p.locked);
			return h1 ^ (h2 << 1) ^ (h3 << 2);
		}
	};

public:
	double x;
	double y;
	bool locked = false;  //当前点是否固定 :[1] locked = false: 不固定；[2] locked = true: 固定 ----默认不固定
private:
	double minNumber = 0.0001;

};

class Line {  // class _declspec(dllexport)Line {
public:
	Line() {}
	Line(Point sP_, Point eP_, Point mP_);
	Line(Point sP_, Point eP_);
	Line(const Point& sP_, const Point& eP_, const int& layer_, const double& width_);
	Line(Point circlePoint, double circleRadius);
	Line(const Line& line); //拷贝构造函数

	double cross(Point a, Point b)const;
	Point rotate(Point vec, double angle);
	double norm(Point a);
	Point getCrossingPoint(const Line& otherLine, bool* parallel = nullptr)const;
	//Point getInsection(Point Vec1, Point p1, Point Vec2, Point p2);
	void getCenterP(Line line);
	Point getVector()const;
	double getLength()const;
	double distanceToLine(const Line& other) const;
	bool intersects(const Line& other) const;
	double distanceToPoint(const Point& p)const;

	bool operator==(Line line1) const;
	// 对起点的操作
	Point& getPt1() {
		return this->Pt1;
	}

	const Point& getPt1() const {
		return this->Pt1;
	}

	void setPt1(const Point& pt1) {
		Line::Pt1 = pt1;
	}

	// 对终点的操作
	Point& getPt2() {
		return this->Pt2;
	}

	const Point& getPt2() const {
		return this->Pt2;
	}

	void setPt2(const Point& pt2) {
		Line::Pt2 = pt2;
	}
	// 对圆弧的中点的操作
	Point& getarcP() {
		return this->arcP;
	}

	void setarcP(const Point& arcP) {
		Line::arcP = arcP;
	}


	double& getRadius() {
		return this->Radius;
	}

	int& getState() {
		return this->State;
	}

	void setState(const int& state) {
		Line::State = state;
	}

public:
	friend std::ostream& operator<<(std::ostream& print, const Line& straightLine) {
		print << "The properties of the Line are:\n";
		print << "Origin Point:" << straightLine.Pt1 << "\n";
		print << "Terminal Point:" << straightLine.Pt2 << "\n";
		if (straightLine.State == 1) {
			print << "Arc Point:" << straightLine.arcP << "\n";
			print << "circleCenter Point:" << straightLine.circleCenter << "\n";
			print << "Radius:" << straightLine.Radius << "\n";
		}
		return print;
	}

public:
	Point Pt1;  //起点
	Point Pt2;  // 终点
	int layer = 0;
	Point arcP;  //圆弧的中点
	Point circleCenter;   //圆心
	double Radius = 0;       //半径
	int State = 0;      // State = 0：segLine; State = 1: arcLine  notice: default = 0; State == 2: Circle
	double width = 0;

};


