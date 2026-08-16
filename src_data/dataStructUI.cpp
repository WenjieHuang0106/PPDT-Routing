#include "src_data/dataStructUI.h"
using namespace std;


vector<QPointF> LineUI::getIntPts(bool withP1, bool withP2) {
	int dist = 1;
	//获取线段上所有整数点，从起点开始
	vector<QPointF> intPts;
	int x1 = (int)this->p1().x(), y1 = (int)this->p1().y();
	int x2 = (int)this->p2().x(), y2 = (int)this->p2().y();
	if (x1 == x2 && y1 == y2) return {};
	// 竖直线，x相等
	if (x1 == x2) {
		int step = (y2 > y1) ? dist : -dist;
		int y = y1;
		if (!withP1) y += step;
		while ((step > 0 && y <= y2) || (step < 0 && y >= y2)) {
			intPts.emplace_back(QPointF(x1, y));
			y += step;
		}
		if (withP2 && intPts.back() != QPointF(x2, y2)) {
			intPts.emplace_back(QPointF(x2, y2));
		}
		return intPts;
	}
	// 水平线，y相等
	if (y1 == y2) {
		int step = (x2 > x1) ? dist : -dist;
		int x = x1;
		if (!withP1) x += step;
		while ((step > 0 && x <= x2) || (step < 0 && x >= x2)) {
			intPts.emplace_back(QPointF(x, y1));
			x += step;
		}
		if (withP2 && intPts.back() != QPointF(x2, y2)) {
			intPts.emplace_back(QPointF(x2, y2));
		}
		return intPts;
	}
	//斜线
	double dx = x2 - x1;
	double dy = y2 - y1;
	double k = dy / dx;
	// 判断斜率绝对值是否 >1，决定按 x 还是 y 步进
	if (abs(k) <= 1) {
		// 平缓线段，按 x 步进
		int step = (x2 > x1) ? dist : -dist;
		int x = x1;
		if (!withP1) x += step;
		while ((step > 0 && x <= x2) || (step < 0 && x >= x2)) {
			double y = y1 + k * (x - x1);
			int roundedY = (int)round(y);
			intPts.emplace_back(QPointF(x, roundedY));
			x += step;
		}
	}
	else {
		// 陡峭线段，按 y 步进
		int step = (y2 > y1) ? dist : -dist;
		int y = y1;
		if (!withP1) y += step;
		while ((step > 0 && y <= y2) || (step < 0 && y >= y2)) {
			double x = x1 + (y - y1) / k;
			int roundedX = (int)round(x);
			intPts.emplace_back(QPointF(roundedX, y));
			y += step;
		}
	}
	// 确保 p2 被正确添加（如果 withP2=true）
	if (withP2 && (intPts.empty() || intPts.back() != QPointF(x2, y2))) {
		intPts.emplace_back(QPointF(x2, y2));
	}
	return intPts;
}