#include "Data_dsn.h"
#include <QDebug>
#include <algorithm>

// ============ setPaintData：从内核数据构建 UI 绘图容器 ============
void Data_dsn::setPaintData() {
	if (!m_dbData) return;
	const auto& db = *m_dbData;

	m_boundaryLines.clear();
	m_PinsNet.clear();
	m_PinsPoly.clear();
	m_circles.clear();

	// 1. 边界
	const auto& bnd = db.m_boundary;
	if (bnd.size() > 3) {
		size_t len_3 = bnd.size() - 3;
		for (size_t i = 1; i < len_3; i += 2) {
			QPointF pt1(bnd[i], bnd[i + 1]);
			QPointF pt2(bnd[i + 2], bnd[i + 3]);
			m_boundaryLines.emplace_back(LineUI(pt1, pt2));
		}
		double x_end = bnd[len_3 + 1], y_end = bnd[len_3 + 2];
		double x_start = bnd[1], y_start = bnd[2];
		if (x_end != x_start || y_end != y_start)
			m_boundaryLines.emplace_back(LineUI(x_end, y_end, x_start, y_start));
	}

	// 2. 引脚中心 m_PinsNet，并计算不在 net 中的 obs pin
	// obsPads 是前端临时数据，用前端副本以免修改内核 const 对象
	std::unordered_map<std::string, std::string> obsPads = db.m_pinPads;
	for (const auto& kv : db.m_nets) {
		const auto& net = kv.second;
		m_PinsNet.emplace_back(std::vector<QPointF>());
		for (const std::string& pinName : net.pinNames) {
			auto pit = db.m_pins.find(pinName);
			if (pit != db.m_pins.end()) {
				const auto& p = pit->second;
				m_PinsNet.back().emplace_back(QPointF(p.x, p.y));
				obsPads.erase(pinName);
			} else {
				qDebug() << "pin not found in m_pins:" << QString::fromStdString(pinName);
			}
		}
	}
	// 写回内核的 obs（需要非 const 访问）
	if (auto* dbMutable = const_cast<db::dsn::Data_dsn*>(&db)) {
		dbMutable->m_obsPads = std::move(obsPads);
	}

	// 3. 焊盘（多边形或圆）
	for (const auto& kv : db.m_pinPads) {
		const std::string& shapeName = kv.first;
		const std::string& padName   = kv.second;

		auto pinit = db.m_pins.find(shapeName);
		auto padit = db.m_pads.find(padName);
		if (pinit == db.m_pins.end() || padit == db.m_pads.end())
			continue;

		double pinX = pinit->second.x;
		double pinY = pinit->second.y;
		const auto& shapes = padit->second.shapes;
		if (shapes.empty()) {
			qDebug() << "shapes is empty:" << QString::fromStdString(padName);
			continue;
		}
		for (const auto& shape : shapes) {
			using SType = db::dsn::Shape::Type;
			if (shape.type == SType::Polygon) {
				m_PinsPoly.emplace_back(std::vector<LineUI>());
				size_t n = shape.polyPts.size();
				if (n < 2) continue;
				bool closed = (n > 1 && shape.polyPts.front() == shape.polyPts.back());
				size_t effN = closed ? n - 1 : n;
				if (effN < 2) continue;
				for (size_t k = 0; k < effN; ++k) {
					size_t nk = (k + 1) % effN;
					double x1 = shape.polyPts[k].x + pinX;
					double y1 = shape.polyPts[k].y + pinY;
					double x2 = shape.polyPts[nk].x + pinX;
					double y2 = shape.polyPts[nk].y + pinY;
					if (x1 == x2 && y1 == y2) continue;
					LineUI line(QPointF(x1, y1), QPointF(x2, y2));
					line.layer = shape.layer;
					m_PinsPoly.back().emplace_back(line);
				}
			}
			else if (shape.type == SType::Circle) {
				while ((int)m_circles.size() < shape.layer)
					m_circles.emplace_back(std::vector<CircleUI>());
				CircleUI cc(shape.circleCenter.x + pinX,
				            shape.circleCenter.y + pinY,
				            shape.circleRadius);
				m_circles[shape.layer - 1].emplace_back(cc);
			}
		}
	}

	// 4. 已有布线 → UI
	updatePaintWires();
}

// ============ updatePaintWires ============
void Data_dsn::updatePaintWires() {
	if (!m_dbData) return;
	const auto& db = *m_dbData;
	m_paths.clear();
	m_viaInfos.clear();
	for (const auto& w : db.m_wirings) {
		using WType = db::dsn::Wiring::Type;
		if (w.type == WType::Wire) {
			int layer = w.layer;
			if (layer < 0 || layer > 8) layer = 1;
			while ((int)m_paths.size() < layer)
				m_paths.emplace_back(std::vector<LineUI>());
			LineUI line(QPointF(w.p1.x, w.p1.y), QPointF(w.p2.x, w.p2.y));
			line.width = w.width;
			line.layer = layer;
			m_paths[layer - 1].emplace_back(line);
		}
		else if (w.type == WType::Via) {
			auto viaIt = db.m_pads.find(w.viaName);
			if (viaIt == db.m_pads.end()) continue;
			for (const auto& s : viaIt->second.shapes) {
				while ((int)m_viaInfos.size() < s.layer)
					m_viaInfos.emplace_back(std::vector<CircleUI>());
				CircleUI cc(w.p1.x, w.p1.y, s.circleRadius);
				m_viaInfos[s.layer - 1].emplace_back(cc);
			}
		}
	}
}

// ============ set_canvas_data1 ============
void Data_dsn::set_canvas_data1(
	std::vector<std::vector<QPointF>*>& pts1, std::vector<const PointStyle*>& ss_pts1,
	std::vector<std::vector<LineUI>*>& lines1, std::vector<const LineStyle*>& ss_lines1,
	std::vector<std::vector<LineUI>*>& polys1, std::vector<const PolygonStyle*>& ss_polys1,
	std::vector<std::vector<LineUI>*>& linePts1, std::vector<const PointStyle*>& ss_linePts1,
	std::vector<std::vector<CircleUI>*>& circle1, std::vector<const PolygonStyle*>& ss_circle1
) {
	(void)lines1; (void)ss_lines1; (void)linePts1; (void)ss_linePts1;
	// 1.1 pin centers m_PinsNet
	for (auto& net : m_PinsNet)
		pts1.emplace_back(&net);
	size_t netSum = m_PinsNet.size();
	ss_pts1.reserve(ss_pts1.size() + netSum);
	const auto* stylePt = m_styles.getPointStyle(m_styleInfo.ptPadKey);
	std::fill_n(std::back_inserter(ss_pts1), netSum, stylePt);
	// 1.2 planning points
	pts1.emplace_back(&m_planningPts);
	ss_pts1.emplace_back(m_styles.getPointStyle(m_styleInfo.planningPtKey));

	// 3.1 边界多边形
	polys1.emplace_back(&m_boundaryLines);
	ss_polys1.emplace_back(m_styles.getPolygonStyle(m_styleInfo.boundaryPolyKey));
	// 3.1 多边形焊盘
	if (!m_PinsPoly.empty()) {
		for (size_t i = 0; i < m_PinsPoly.size(); ++i) {
			if (m_PinsPoly[i].empty()) continue;
			polys1.emplace_back(&m_PinsPoly[i]);
			int layer = m_PinsPoly[i].front().layer;
			const auto* stylePoly = m_styles.getPolygonStyle(m_styleInfo.getPadPolyKey(layer - 1));
			ss_polys1.emplace_back(stylePoly);
		}
	}
	// 5.1 圆形焊盘（高层先画）
	for (int i = (int)m_circles.size() - 1; i >= 0; --i) {
		circle1.emplace_back(&m_circles[i]);
		const auto* stylePoly = m_styles.getPolygonStyle(m_styleInfo.getPadPolyKey(i));
		ss_circle1.emplace_back(stylePoly);
	}
}

// ============ set_canvas_data2 ============
void Data_dsn::set_canvas_data2(
	std::vector<std::vector<QPointF>*>& pts2, std::vector<const PointStyle*>& ss_pts2,
	std::vector<std::vector<LineUI>*>& lines2, std::vector<const LineStyle*>& ss_lines2,
	std::vector<std::vector<LineUI>*>& polys2, std::vector<const PolygonStyle*>& ss_polys2,
	std::vector<std::vector<LineUI>*>& linePts2, std::vector<const PointStyle*>& ss_linePts2,
	std::vector<std::vector<CircleUI>*>& circle2, std::vector<const PolygonStyle*>& ss_circle2
) {
	(void)pts2; (void)ss_pts2; (void)polys2; (void)ss_polys2;
	(void)linePts2; (void)ss_linePts2;
	// 2.1 fly lines
	for (auto& lines : m_flyLines)
		lines2.emplace_back(&lines);
	size_t flyLinesSum = m_flyLines.size();
	ss_lines2.reserve(ss_lines2.size() + flyLinesSum);
	const auto* flyStyle = m_styles.getLineStyle(m_styleInfo.flyLineKey);
	std::fill_n(std::back_inserter(ss_lines2), flyLinesSum, flyStyle);
	// 2.2 路径 (高层先画)
	for (int i = (int)m_paths.size() - 1; i >= 0; --i) {
		lines2.emplace_back(&m_paths[i]);
		const auto* pathStyle = m_styles.getLineStyle(m_styleInfo.getPathLineKey(i));
		ss_lines2.emplace_back(pathStyle);
	}
	// 5.1 via circles
	circle2.emplace_back(&m_viaCircles);
	const auto* viaStyle = m_styles.getPolygonStyle(m_styleInfo.viaCicleKey);
	ss_circle2.emplace_back(viaStyle);
}

// ============ set_canvas_data3 ============
void Data_dsn::set_canvas_data3(
	std::vector<std::vector<LineUI>*>& lines3, std::vector<const LineStyle*>& ss_lines3
) {
	for (auto& lines : m_treesLines)
		lines3.emplace_back(&lines);
	size_t treeSum = m_treesLines.size();
	ss_lines3.reserve(ss_lines3.size() + treeSum);
	const auto* treeStyle = m_styles.getLineStyle(m_styleInfo.treeLineKey);
	std::fill_n(std::back_inserter(ss_lines3), treeSum, treeStyle);
}
