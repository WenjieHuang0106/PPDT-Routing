#pragma once
#include <QPen>
#include <QBrush>
#include <QHash>
#include <vector>

// 图形样式基类
class DiagramStyle {
public:
	virtual ~DiagramStyle() = default;
	bool isVisible() const { return m_visible; };
	void setVisible(bool visible) { m_visible = visible; };

private:
	bool m_visible = true;

public:
	virtual QPen getPen() const = 0;
	virtual QBrush getBrush() const = 0;

};

//m_type选中标记说明：m_type缺省为0
// 0不可选中
// 1-999完全可选中
// 1001以上为线段只能选中线段
// 2001以上为线段只能选中端点
// 点/圆样式
class PointStyle : public DiagramStyle {
public:
	QColor m_outlineColor;	// 外框颜色
	bool m_outlineShow;		// 外框是否可见
	float m_outlineWidth;	// 外框宽度
	bool m_fill;			// 填充
	QColor m_fillColor;		// 填充颜色
	float m_r;				// 半径
	int m_type;				// 分类，以便确定是否捕捉
public:
	explicit PointStyle(
		const QColor& outlineColor = Qt::black,
		bool outlineShow = true,
		float outlineWidth = 1.0,
		bool fill = false,
		const QColor& fillColor = Qt::transparent,
		float r = 3.0,
		int type = 0
	) :
		m_outlineColor(outlineColor),
		m_outlineShow(outlineShow),
		m_outlineWidth(outlineWidth),
		m_fill(fill),
		m_fillColor(fillColor),
		m_r(r),
		m_type(type) {
	};

	QPen getPen() const override {
		return QPen(m_outlineColor, m_outlineWidth);
	}

	QBrush getBrush() const override {
		return m_fill ? QBrush(m_fillColor) : QBrush(Qt::NoBrush);
	}

	float radius() const { return m_r; }


};


// 线段样式
class LineStyle : public DiagramStyle {
public:
	QColor m_color;			// 颜色
	double m_width;			// 线宽
	Qt::PenStyle m_style;	// 线型（实线、虚线等）
	int m_type;				// 分类，以便确定是否捕捉

public:
	explicit LineStyle(
		const QColor& color = Qt::black,
		float width = 1.0f,
		Qt::PenStyle style = Qt::SolidLine,
		int type = 0
	) :
		m_color(color),
		m_width(width),
		m_style(style),
		m_type(type) {
	}
	QPen getPen() const override {
		return QPen(m_color, m_width, m_style);
	}
	QBrush getBrush() const override { return QBrush(Qt::NoBrush); }

};

// 多边形样式
class PolygonStyle : public DiagramStyle {
public:
	QColor m_outlineColor;	// 外框颜色
	bool m_outlineShow;		// 外框是否可见
	float m_outlineWidth;	// 外框宽度
	bool m_fill;			// 填充
	QColor m_fillColor;		// 填充颜色
	int m_type;
public:
	explicit PolygonStyle(
		const QColor& outlineColor = Qt::black,
		bool outlineShow = true,
		float outlineWidth = 1.0f,
		bool fill = false,
		const QColor& fillColor = Qt::blue,
		int type = 0
	) :
		m_outlineColor(outlineColor),
		m_outlineShow(outlineShow),
		m_outlineWidth(outlineWidth),
		m_fill(fill),
		m_fillColor(fillColor),
		m_type(type) {
	}

	QPen getPen() const override {
		return QPen(m_outlineColor, m_outlineWidth);
	}

	QBrush getBrush() const override {
		return m_fill ? QBrush(m_fillColor) : QBrush(Qt::NoBrush);
	}
};


class DiagramStyleManager {
public:
	DiagramStyleManager() {
		m_pointStyles[""] = PointStyle();
		m_lineStyles[""] = LineStyle();
		m_polygonStyles[""] = PolygonStyle();
	};
	~DiagramStyleManager() = default;

	// 获取样式
	PointStyle* getPointStyle(const QString& key);
	LineStyle* getLineStyle(const QString& key);
	PolygonStyle* getPolygonStyle(const QString& key);

	//设置样式
	void setPointStyle(const QString& key, const PointStyle& style);
	void setPointStyle(const std::vector<QString>& keys, const std::vector<PointStyle>& styles);
	void setLineStyle(const QString& key, const LineStyle& style);
	void setLineStyle(const std::vector<QString>& keys, const std::vector<LineStyle>& styles);
	void setPolygonStyle(const QString& key, const PolygonStyle& style);
	void setPolygonStyle(const std::vector<QString>& keys, const std::vector<PolygonStyle>& styles);

private:
	QHash<QString, PointStyle> m_pointStyles;
	QHash<QString, LineStyle> m_lineStyles;
	QHash<QString, PolygonStyle> m_polygonStyles;

};
