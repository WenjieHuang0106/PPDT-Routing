#pragma once
#include <QLine>
#include <QString>

const double const_minValue = 0.001;

class CircleUI : public QPointF {
public:
	CircleUI() : QPointF(), radius(1) {}
	CircleUI(double r) : QPointF(), radius(r) {}
	CircleUI(qreal x, qreal y, double r = 0)
		: QPointF(x, y), radius(r) {
	}
	CircleUI(const QPointF& pt, double r = 0)
		: QPointF(pt), radius(r) {
	}
public:
	double radius;
};


class LineUI : public QLineF {
public:
	LineUI() : QLineF(), width(0) {}
	LineUI(qreal x1, qreal y1, qreal x2, qreal y2, int layer = 0, double w = 0)
		: QLineF(x1, y1, x2, y2), layer(layer), width(w) {
	}
	LineUI(const QPointF& p1, const QPointF& p2, double w = 0)
		: QLineF(p1, p2), width(w) {
	}
	LineUI(const QLineF& line, double w = 0)
		: QLineF(line), width(w) {
	}
	LineUI(const LineUI& line, int layer, double w = 0)
		: QLineF(line), layer(layer), width(w) {
	}

public:
	double width = 0;
	int layer = 0;
	std::vector<QPointF> getIntPts(bool withP1 = true, bool withP2 = true);	//获取线段上所有整数点，从起点开始
};

struct SelectedTarget {
	bool pinSelected = false;
	bool lineSelected = false;
	bool isMoving = false;		// 鼠标正在移动

	LineUI sLine;				// 选中的线段
	QPointF nearistPt;			// 捕捉最近的点
	std::vector<LineUI>* linesToRun = nullptr;		// 选中的线段所在的折线
	QPointF offset;				// 鼠标偏移量

	//显示的信息
	QString infoText;           // 要显示的信息
	QPointF infoPosition;       // 信息显示位置
	bool showInfo = false;

	SelectedTarget() {
		clear();
	}
	void clear() {
		pinSelected = false;
		lineSelected = false;
		isMoving = false;
		sLine = LineUI(0, 0, 0, 0);
		nearistPt.setX(0);
		nearistPt.setY(0);
		linesToRun = nullptr;
		offset.setX(0);
		offset.setY(0);
		infoText.clear();
		showInfo = false;
	}
};
