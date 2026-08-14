#pragma once
#include "../../src_baseClasses/Data.h"
#include "../../src_baseClasses/DiagramData.h"
#include "Style_dsn.h"
#include <QHash>
#include <QSet>

class Data_dsn : public Data, public DiagramData {
public:
	Data_dsn() { setupStyleManager(); }
	~Data_dsn() {}

	struct DSNComponent {
		QPointF origin;		//元件原点坐标，内存16B
		QString direction;	//元件方向，内存8B
		double rotation;	//元件旋转角度，内存8B

	};
	struct DSNShape {
		std::vector<QPointF> Pts;	//存储形状参数，如圆形的直径，矩形的长宽等（以几何中心为坐标原点）
		CircleUI circle;
		QSet<QString> PinsName;		//与该焊盘关联的引脚名称（可省略）
		QString shapeType;		//形状类型，如circle，polygon
		double rotation;		//旋转角度
		int layer = 1;			//所在层
		int backup;				//备用字段，内存对齐
	};
	struct DSNPad {
		std::vector<DSNShape> shapes;	//焊盘形状集合
		QSet<QString> PinsName;			//网对应的引脚名
	};
	struct DSNNet {
		std::vector<QString> PinsNames;	//引脚对应的焊盘的坐标（需要计算得出）
		QString via = "via0";		//默认孔（可省略）
		double width = 1;			//线宽（可省略）
		double clearance = 0.4;		//间距（可省略）
	};
	struct DSNWring {
		LineUI line;			//线段，内部有线宽，内存40B
		QString wiringType;		//线段类型，如wire，via等,内存8B
		QString viaName;		//过孔名，如via0,via1等,内存8B
		QString netName;		//所属网络,内存8B
		QString type;			//线段类型，如protect等,内存8B
		int layer;				//所在层,内存4B
	};

	//直接读取的信息
	QHash<QString, QString> m_dsnPCBInfo;			//dsn标题，导出的CAD工具，工具版本号
	double m_dsnResolution = 100;					//dsn分辨率 (mil)

	std::vector<double> m_dsnBoundary;				//dsn边界数据
	QSet<QString> m_dsnVias;						//dsn孔定义
	QHash<QString, double>	m_dsnGride;				//网格吸附间距place,wire,via
	QHash<QString, double> m_dsnRule;				//间距default,default_smd,smd_smd
	double m_ruleWidth = 1.0;						//默认线宽
	QHash<int, QString> m_dsnLayers;				//dsn层名,0层signal，1层signal

	QHash<QString, DSNComponent> m_dsnComponent;	//placement

	QHash<QString, DSNPad> m_dsnPads;				//焊盘（含形状信息）
	QHash<QString, QPointF>	m_dsnPins;				//引脚（含坐标信息）
	QHash<QString, QString> m_dsnPinPads;			//引脚名-焊盘名映射关系
	QHash<QString, QString> m_obssPads;				//不在net中的引脚-焊盘

	QHash<QString, DSNNet> m_dsnNets;				//网络（含引脚信息）
	std::vector<DSNWring> m_dsnWirings;				//线段（含布线信息）

	//绘制与交互的信息
	double m_trans = 1;			//0.254可转化为mm
	std::vector<LineUI> m_boundaryLines;			//边界
	std::vector<std::vector<QPointF>> m_PinsNet;	//引脚（焊盘中心）
	std::vector<std::vector<LineUI>> m_PinsPoly;	//多边形焊盘
	std::vector<std::vector<CircleUI>> m_circles;	//圆形焊盘，每个容器是一层
	std::vector<CircleUI> m_viaCircles;				//圆形过孔，每个容器是一层

	std::vector<QPointF> m_planningPts;				//规划点
	std::vector<std::vector<LineUI>> m_paths;		//走线，初始读取数据时，每个容器是一层
	std::vector<std::vector<LineUI>> m_treesLines;	//搜索树
	std::vector<std::vector<CircleUI>> m_viaInfos;	//过孔
	std::vector<std::vector<LineUI>> m_flyLines;	//飞线

	void transform() { // 将相关数据进行单位转换，mil*0.254得到毫米
		minPt.setX(minPt.x() * m_trans);
		minPt.setY(minPt.y() * m_trans);
		maxPt.setX(maxPt.x() * m_trans);
		maxPt.setY(maxPt.y() * m_trans);
		// 1. 转换 m_dsnBoundary（从第1个数字开始）
		if (m_dsnBoundary.size() > 1) {
			std::transform(m_dsnBoundary.begin() + 1, m_dsnBoundary.end(),
				m_dsnBoundary.begin() + 1,
				[this](double x) { return x * m_trans; });
		}

		// 2. 转换 m_dsnGride
		for (auto it = m_dsnGride.begin(); it != m_dsnGride.end(); ++it) {
			it.value() *= m_trans;
		}

		// 3. 转换 m_dsnRule
		for (auto it = m_dsnRule.begin(); it != m_dsnRule.end(); ++it) {
			it.value() *= m_trans;
		}

		// 4. 转换 m_ruleWidth
		m_ruleWidth *= m_trans;

		// 5. 转换 m_dsnPads
		for (auto it = m_dsnPads.begin(); it != m_dsnPads.end(); ++it) {
			DSNPad& pad = it.value();
			for (DSNShape& shape : pad.shapes) {
				// 转换 Pts 中的所有坐标点
				if (shape.shapeType == "polygon")
					for (QPointF& point : shape.Pts) {
						point.setX(point.x() * m_trans);
						point.setY(point.y() * m_trans);
					}
				else if (shape.shapeType == "circle") {
					CircleUI& cc = shape.circle;
					cc.setX(cc.x() * m_trans);
					cc.setY(cc.y() * m_trans);
					cc.radius *= m_trans;	// 将读取的直径转化为毫米，并计算半径
				}

			}
		}

		// 6. 转换 m_dsnPins
		for (auto it = m_dsnPins.begin(); it != m_dsnPins.end(); ++it) {
			QPointF& point = it.value();
			point.setX(point.x() * m_trans);
			point.setY(point.y() * m_trans);
		}

		// 7. 转换 m_dsnNets
		for (auto it = m_dsnNets.begin(); it != m_dsnNets.end(); ++it) {
			DSNNet& net = it.value();
			net.width *= m_trans;
			net.clearance *= m_trans;
		}

		// 8. 转换 m_dsnWirings
		for (DSNWring& wiring : m_dsnWirings) {
			// 如果需要转换 LineUI 中的坐标，可以在这里添加
			QPointF p1 = wiring.line.p1();  // 获取起点
			QPointF p2 = wiring.line.p2();  // 获取终点

			p1.setX(p1.x() * m_trans);
			p1.setY(p1.y() * m_trans);
			p2.setX(p2.x() * m_trans);
			p2.setY(p2.y() * m_trans);

			// 设置转换后的坐标
			wiring.line.setP1(p1);
			wiring.line.setP2(p2);

			// 同时转换 LineUI 自己的 width 成员
			wiring.line.width *= m_trans;
		}
	}

	void clear() {
		m_boundaryLines.clear();
		m_PinsNet.clear();
		m_PinsPoly.clear();
		m_circles.clear();
		m_planningPts.clear();
		m_paths.clear();
		m_treesLines.clear();
		m_viaInfos.clear();
		m_flyLines.clear();
	}
	void setPaintData() {
		// 设置读取的数据中，要绘制的部分
		m_boundaryLines.clear();
		m_PinsNet.clear();
		m_PinsPoly.clear();
		m_circles.clear();
		//1.边界m_boundaryLines
		size_t len_3 = m_dsnBoundary.size() - 3;
		for (int i = 1; i < len_3; i += 2) {
			QPointF pt1(m_dsnBoundary[i], m_dsnBoundary[i + 1]);
			QPointF pt2(m_dsnBoundary[i + 2], m_dsnBoundary[i + 3]);
			m_boundaryLines.emplace_back(LineUI(pt1, pt2));
		}
		double x_end = m_dsnBoundary[len_3 + 1], y_end = m_dsnBoundary[len_3 + 2]; //封闭图形
		double x_start = m_dsnBoundary[1], y_start = m_dsnBoundary[2];
		if (x_end != x_start || y_end != y_start)
			m_boundaryLines.emplace_back(LineUI(x_end, y_end, x_start, y_start));

		//2.引脚中心坐标m_PinsNet，并设置禁布区（不在net中的pin）
		m_obssPads = m_dsnPinPads;
		for (auto& net : m_dsnNets) {
			m_PinsNet.emplace_back(std::vector<QPointF>());
			for (QString& shapeName : net.PinsNames) {
				if (m_dsnPins.contains(shapeName)) {
					m_PinsNet.back().emplace_back(m_dsnPins[shapeName]);
					m_obssPads.remove(shapeName);
				}
				else {
					qDebug() << "shapeName not found in Pins:" << shapeName;
				}
			}
		}

		//3.焊盘（多边形或圆）
		for (auto it = m_dsnPinPads.begin(); it != m_dsnPinPads.end(); ++it) {
			const QString& shapeName = it.key();
			const QString& padName = it.value();

			if (!m_dsnPins.contains(shapeName) || !m_dsnPads.contains(padName))
				continue;

			// 2.2添加pad的多边形
			std::vector<DSNShape>& shapes = m_dsnPads[padName].shapes;
			if (shapes.empty()) {
				qDebug() << "shapes is empty:" << padName;
				continue;
			}
			double pinX = m_dsnPins[shapeName].x();
			double pinY = m_dsnPins[shapeName].y();
			for (const DSNShape& shape : shapes) {
				if (shape.shapeType == "polygon") {
					m_PinsPoly.emplace_back(std::vector<LineUI>());
					double x1 = shape.Pts[0].x(), y1 = shape.Pts[0].y();
					double x2 = shape.Pts[1].x(), y2 = shape.Pts[1].y();
					int layer = shape.layer;
					size_t pointCount = shape.Pts.size();
					bool isClosed = (pointCount > 1 && shape.Pts[0] == shape.Pts[pointCount - 1]);
					size_t effectivePointCount = isClosed ? pointCount - 1 : pointCount;
					if (effectivePointCount < 2) continue;
					for (size_t i = 0; i < effectivePointCount; ++i) {
						size_t next_i = (i + 1) % effectivePointCount;
						double x1 = shape.Pts[i].x() + pinX;
						double y1 = shape.Pts[i].y() + pinY;
						double x2 = shape.Pts[next_i].x() + pinX;
						double y2 = shape.Pts[next_i].y() + pinY;
						if (x1 == x2 && y1 == y2) {
							continue;
						}
						LineUI line(QPointF(x1, y1), QPointF(x2, y2));
						line.layer = layer;
						m_PinsPoly.back().emplace_back(line);
					}
				}
				else if (shape.shapeType == "circle") {
					while (shape.layer > m_circles.size())
						m_circles.emplace_back(std::vector<CircleUI>());
					CircleUI ccPin(shape.circle.x() + pinX, shape.circle.y() + pinY, shape.circle.radius);
					m_circles[shape.layer - 1].emplace_back(ccPin);
				}
			}
		}
		//4.读取出来的布线数据
		updatePaintWires();
	}
	void updatePaintWires() {
		// 读取出来的走线数据
		m_paths.clear();
		m_viaInfos.clear();
		for (DSNWring& wire : m_dsnWirings) {
			if (wire.wiringType == "wire") {
				if (wire.layer < 0 || wire.layer > 8) wire.layer = 1;
				while (wire.layer > m_paths.size()) m_paths.emplace_back(std::vector<LineUI>());
				m_paths[wire.layer - 1].emplace_back(wire.line);
			}
			else if (wire.wiringType == "via") {
				if (m_dsnPads.contains(wire.viaName)) {
					std::vector<DSNShape>& shapes = m_dsnPads[wire.viaName].shapes;
					for (auto& shape : shapes) {
						shape.circle.setX(wire.line.p1().x());
						shape.circle.setY(wire.line.p1().y());
						while (shape.layer > m_viaInfos.size()) m_viaInfos.emplace_back(std::vector<CircleUI>());
						m_viaInfos[shape.layer - 1].emplace_back(shape.circle);
					}
				}
			}
		}
	}

	// 实现DiagramData接口
	QPointF* getMinPoint() override { return &minPt; }
	QPointF* getMaxPoint() override { return &maxPt; }

	// 1.设置画布信息(优先绘制的内容)
	void set_canvas_data1(
		std::vector<std::vector<QPointF>*>& pts1, std::vector<PointStyle*>& ss_pts1,
		std::vector<std::vector<LineUI>*>& lines1, std::vector<LineStyle*>& ss_lines1,
		std::vector<std::vector<LineUI>*>& polys1, std::vector<PolygonStyle*>& ss_polys1,
		std::vector<std::vector<LineUI>*>& linePts1, std::vector<PointStyle*>& ss_linePts1,
		std::vector<std::vector<CircleUI>*>& circle1, std::vector<PolygonStyle*>& ss_circle1
	) {
		//1.1 点：pin,m_PinsNet(焊盘中心点)
		for (auto& net : m_PinsNet)
			pts1.emplace_back(&net);
		size_t netSum = m_PinsNet.size();
		ss_pts1.reserve(ss_pts1.size() + netSum);
		const auto& stylePt = m_styles.getPointStyle(m_styleInfo.ptPadKey);
		std::fill_n(std::back_inserter(ss_pts1), netSum, stylePt);
		//1.2 点：planningPtKey规划点
		pts1.emplace_back(&m_planningPts);
		ss_pts1.emplace_back(m_styles.getPointStyle(m_styleInfo.planningPtKey));

		//2.1 线：
		//3.1 多边形：边界
		polys1.emplace_back(&m_boundaryLines);
		ss_polys1.emplace_back(m_styles.getPolygonStyle(m_styleInfo.boundaryPolyKey));
		//3.1 多边形：多边形焊盘
		if (!m_PinsPoly.empty()) {
			for (int i = 0; i < m_PinsPoly.size(); i++) {
				polys1.emplace_back(&m_PinsPoly[i]);
				int layer = m_PinsPoly[i].front().layer;
				const auto& stylePoly = m_styles.getPolygonStyle(m_styleInfo.getPadPolyKey(layer - 1));
				ss_polys1.emplace_back(stylePoly);
			}
		}
		//4.1 线段端点：
		//5.1 圆：圆形焊盘
		for (int i = (int)m_circles.size() - 1; i >= 0; i--) {
			circle1.emplace_back(&m_circles[i]);
			const auto& stylePoly = m_styles.getPolygonStyle(m_styleInfo.getPadPolyKey(i));
			ss_circle1.emplace_back(stylePoly);
		}
	};
	// 2.设置前景绘图信息(最后绘制的内容，会将先绘制的内容挡住)
	void set_canvas_data2(
		std::vector<std::vector<QPointF>*>& pts2, std::vector<PointStyle*>& ss_pts2,
		std::vector<std::vector<LineUI>*>& lines2, std::vector<LineStyle*>& ss_lines2,
		std::vector<std::vector<LineUI>*>& polys2, std::vector<PolygonStyle*>& ss_polys2,
		std::vector<std::vector<LineUI>*>& linePts2, std::vector<PointStyle*>& ss_linePts2,
		std::vector<std::vector<CircleUI>*>& circle2, std::vector<PolygonStyle*>& ss_circle2
	) {
		//1.1 点：
		//2.1 线：飞线
		for (auto& lines : m_flyLines)
			lines2.emplace_back(&lines);
		size_t flyLinesSum = m_flyLines.size();
		ss_lines2.reserve(ss_lines2.size() + flyLinesSum);
		const auto& flyLinesStyle = m_styles.getLineStyle(m_styleInfo.flyLineKey);
		std::fill_n(std::back_inserter(ss_lines2), flyLinesSum, flyLinesStyle);

		//2.2 线：走线
		for (int i = (int)m_paths.size() - 1; i >= 0; i--) {
			lines2.emplace_back(&m_paths[i]);
			const auto& pathStyle = m_styles.getLineStyle(m_styleInfo.getPathLineKey(i));
			ss_lines2.emplace_back(pathStyle);
		}

		//3.1 多边形：
		//4.1 线段端点：
		//5.1 圆：圆形焊盘
		circle2.emplace_back(&m_viaCircles);
		const auto& styleVia = m_styles.getPolygonStyle(m_styleInfo.viaCicleKey);
		ss_circle2.emplace_back(styleVia);
	};
	// 3.设置动态绘图信息(下拉选择显示哪个数据)
	void set_canvas_data3(
		std::vector<std::vector<LineUI>*>& lines3, std::vector<LineStyle*>& ss_lines3
	) {
		//1 线：搜索树
		for (auto& lines : m_treesLines)
			lines3.emplace_back(&lines);
		size_t treeLinesSum = m_treesLines.size();
		ss_lines3.reserve(ss_lines3.size() + treeLinesSum);
		const auto& treeLinesStyle = m_styles.getLineStyle(m_styleInfo.treeLineKey);
		std::fill_n(std::back_inserter(ss_lines3), treeLinesSum, treeLinesStyle);
	};

public:
	//定义绘图样式
	Style_dsn m_styleInfo = Style_dsn();
private:

	void setupStyleManager() {
		m_styles.setPointStyle(m_styleInfo.pointStyleKeys, m_styleInfo.pointStyles);
		m_styles.setLineStyle(m_styleInfo.lineStyleKeys, m_styleInfo.lineStyles);
		m_styles.setPolygonStyle(m_styleInfo.polygonStyleKeys, m_styleInfo.polygonStyles);
	}

};
