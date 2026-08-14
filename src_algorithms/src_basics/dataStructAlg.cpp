#include "dataStructAlg.h"

Line::Line(Point sP_, Point eP_, Point mP_) { //圆弧
	Line line;
	line.Pt1 = this->Pt1 = sP_;
	line.Pt2 = this->Pt2 = eP_;
	line.arcP = this->arcP = mP_;
	line.State = this->State = 1;
	getCenterP(line);
}

Line::Line(Point sP_, Point eP_) { //线段
	this->Pt1 = sP_;
	this->Pt2 = eP_;
	this->arcP = eP_;
	this->State = 0;
}
Line::Line(const Point& sP_, const Point& eP_, const int& layer_, const double& width_)
	:Pt1(sP_), Pt2(eP_), layer(layer_), width(width_) {
}


Line::Line(Point circlePoint, double circleRadius) {   // 圆
	this->circleCenter = circlePoint;
	this->Radius = circleRadius;
	this->State = 2;
}
Line::Line(const Line& line)
	:Pt1(line.Pt1), Pt2(line.Pt2), layer(line.layer), width(line.width),
	arcP(line.arcP), circleCenter(line.circleCenter), Radius(line.Radius), State(line.State) {
}

double Line::cross(Point a, Point b)const {    //叉乘
	return a.x * b.y - a.y * b.x;
}


Point Line::rotate(Point vec, double angle) {
	double rad = angle * PI / 180.0;
	Point rotate_vec = Point(vec.x * cos(rad) - vec.y * sin(rad), vec.y * cos(rad) + vec.x * sin(rad));
	return rotate_vec;
}

double Line::norm(Point a) {    //向量的模
	return sqrt(a.x * a.x + a.y * a.y);
}

Point Line::getVector() const {    //向量
	return (this->Pt2 - this->Pt1);
}


//计算圆弧时的圆心
void Line::getCenterP(Line line) {
	//cout << "@@@@@@@@@@@@@@@@@@@@@@@  Enter to get arc's circlePoint: " << endl;
	Point sP_, eP_, mP_;
	Point meVec, seVec, mcVec, vec1, meVecVertical;
	Point meVecnorm, seVecnorm, mcVecnorm, ceVecnorm;
	Point midSE, midME;
	Point circleCentralPoint;  //y圆心

	auto getCrossingPoint = [&](Point Vec1, Point p1, Point Vec2, Point p2) {
		Point P1P2 = p2 - p1;
		double t = cross(P1P2, Vec2) / cross(Vec1, Vec2);
		return p1 + Vec1 * t;
		};
	auto PointLinearDist = [](Point point1, Point point2) {
		double distance;
		distance = sqrt((point1.x - point2.x) * (point1.x - point2.x) + (point1.y - point2.y) * (point1.y - point2.y));
		return distance;
		};
	//初始化
	sP_ = line.Pt1;
	eP_ = line.Pt2;
	mP_ = line.arcP;
	midSE = Point((sP_.x + eP_.x) / 2, (sP_.y + eP_.y) / 2);
	midME = Point((mP_.x + eP_.x) / 2, (mP_.y + eP_.y) / 2);

	vec1 = mP_ - midSE; //判断用
	meVec = eP_ - mP_;
	seVec = eP_ - sP_;
	mcVec = rotate(seVec, -90);  //法向量
	meVecVertical = rotate(meVec, -90);

	circleCentralPoint = getCrossingPoint(meVecVertical, midME, mcVec, midSE);

	this->circleCenter = circleCentralPoint;
	this->Radius = PointLinearDist(circleCentralPoint, sP_);
}
Point Line::getCrossingPoint(const Line& otherLine, bool* parallel) const {
	Point Vec1 = this->Pt2 - this->Pt1;
	Point Vec2 = otherLine.Pt2 - otherLine.Pt1;
	double crossValue = cross(Vec1, Vec2);
	if (std::abs(crossValue) < MapMinValue) {
		double distOfPt1ToPt1 = this->Pt1.distanceTo(otherLine.Pt1);
		double distOfPt1ToPt2 = this->Pt1.distanceTo(otherLine.Pt2);
		if (parallel) *parallel = true;
		return (distOfPt1ToPt1 < distOfPt1ToPt2) ? otherLine.Pt1 : otherLine.Pt2;
	}
	if (parallel) *parallel = false;

	Point P1P2 = otherLine.Pt2 - this->Pt2;
	double t = cross(P1P2, Vec2) / crossValue;
	return this->Pt2 + Vec1 * t;
}

double Line::getLength() const {
	Point p = this->Pt2 - this->Pt1;
	return sqrt(p.x * p.x + p.y * p.y);
}
double Line::distanceToLine(const Line& other)const {
	if (this->intersects(other)) {
		return 0;
	}
	// 计算四个端点到另一条线段的最小距离
	double minDist = std::numeric_limits<double>::max();
	minDist = std::min(minDist, this->distanceToPoint(other.Pt1));
	minDist = std::min(minDist, this->distanceToPoint(other.Pt2));
	minDist = std::min(minDist, other.distanceToPoint(this->Pt1));
	minDist = std::min(minDist, other.distanceToPoint(this->Pt2));

	// 如果距离小于阈值，返回0
	return (minDist < MapMinValue) ? 0 : minDist;
}
bool Line::intersects(const Line& other) const {
	Point p1 = this->Pt1, p2 = this->Pt2;
	Point p3 = other.Pt1, p4 = other.Pt2;

	// 计算方向向量的叉积
	auto cross = [](const Point& a, const Point& b, const Point& c) {
		return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
		};

	double d1 = cross(p3, p4, p1);
	double d2 = cross(p3, p4, p2);
	double d3 = cross(p1, p2, p3);
	double d4 = cross(p1, p2, p4);

	// 标准相交检查
	if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
		((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) {
		return true;
	}

	// 检查共线情况
	auto isOnSegment = [](const Point& p, const Line& seg) {
		if (p.x < std::min(seg.Pt1.x, seg.Pt2.x) - MapMinValue ||
			p.x > std::max(seg.Pt1.x, seg.Pt2.x) + MapMinValue ||
			p.y < std::min(seg.Pt1.y, seg.Pt2.y) - MapMinValue ||
			p.y > std::max(seg.Pt1.y, seg.Pt2.y) + MapMinValue) {
			return false;
		}
		return true;
		};

	if (std::abs(d1) < MapMinValue && isOnSegment(p1, other)) return true;
	if (std::abs(d2) < MapMinValue && isOnSegment(p2, other)) return true;
	if (std::abs(d3) < MapMinValue && isOnSegment(p3, *this)) return true;
	if (std::abs(d4) < MapMinValue && isOnSegment(p4, *this)) return true;

	return false;
}

double Line::distanceToPoint(const Point& p) const {
	Point v = this->Pt2 - this->Pt1;
	Point w = p - this->Pt1;

	double c1 = w.x * v.x + w.y * v.y;  // 点积 w·v
	if (c1 <= 0) {
		// 最近点是起点
		return std::sqrt(w.x * w.x + w.y * w.y);
	}

	double c2 = v.x * v.x + v.y * v.y;  // v·v
	if (c2 <= c1) {
		// 最近点是终点
		Point w2 = p - this->Pt2;
		return std::sqrt(w2.x * w2.x + w2.y * w2.y);
	}

	// 最近点是投影点
	double b = c1 / c2;
	Point pb = this->Pt1 + v * b;
	Point distVec = p - pb;
	return std::sqrt(distVec.x * distVec.x + distVec.y * distVec.y);
}

bool Line::operator==(Line line1) const
{
	if (State != line1.State) {
		return false;
	}
	else if ((State == line1.State) && (State == 0)) {  //线段
		return ((Pt1 == line1.Pt1 && Pt2 == line1.Pt2) || (Pt2 == line1.Pt1 && Pt1 == line1.Pt2));
	}
	else if ((State == line1.State) && (State == 1)) {  //圆弧
		return (((Pt1 == line1.Pt1 && Pt2 == line1.Pt2) || (Pt2 == line1.Pt1 && Pt1 == line1.Pt2)) && arcP == line1.arcP);
	}
	else {
		return false;
	}
}

