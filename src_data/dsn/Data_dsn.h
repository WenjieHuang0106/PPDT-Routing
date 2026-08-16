#pragma once
#include "../DataBase.h"
#include "../DiagramDataBase.h"
#include "Style_dsn.h"
#include "../../src_db/dsn/dbData_dsn.h"
#include <memory>
#include <QPointF>

class Data_dsn : public DataBase, public DiagramDataBase {
public:
	Data_dsn() { setupStyleManager(); }
	~Data_dsn() override = default;

	// ============ 内核数据关联 ============
	void attachDbData(std::unique_ptr<db::dsn::Data_dsn>&& d) {
		m_dbData = std::move(d);
		m_cachedMinPt = QPointF(-2, -2);
		m_cachedMaxPt = QPointF(-1, -1);
	}
	db::dsn::Data_dsn*       dbData()       { return m_dbData.get(); }
	const db::dsn::Data_dsn* dbData() const { return m_dbData.get(); }
	bool hasDbData() const { return m_dbData != nullptr; }

	// ============ UI 绘图容器 ============
	std::vector<LineUI> m_boundaryLines;
	std::vector<std::vector<QPointF>> m_PinsNet;
	std::vector<std::vector<LineUI>> m_PinsPoly;
	std::vector<std::vector<CircleUI>> m_circles;
	std::vector<CircleUI> m_viaCircles;

	std::vector<QPointF> m_planningPts;
	std::vector<std::vector<LineUI>> m_paths;
	std::vector<std::vector<LineUI>> m_treesLines;
	std::vector<std::vector<CircleUI>> m_viaInfos;
	std::vector<std::vector<LineUI>> m_flyLines;

	// ============ 主要流程函数 ============
	// 清空 UI 绘图容器
	void clear() {
		m_boundaryLines.clear();
		m_PinsNet.clear();
		m_PinsPoly.clear();
		m_circles.clear();
		m_viaCircles.clear();
		m_planningPts.clear();
		m_paths.clear();
		m_treesLines.clear();
		m_viaInfos.clear();
		m_flyLines.clear();
	}

	// 从内核数据构建 UI 绘图容器
	void setPaintData();

	// 把读取出来的布线数据更新到 UI 容器
	void updatePaintWires();

	// ============ 实现 DiagramDataBase 接口 ============
	QPointF* getMinPoint() override { syncBBoxCache(); return &m_cachedMinPt; }
	QPointF* getMaxPoint() override { syncBBoxCache(); return &m_cachedMaxPt; }

	// 1.设置画布信息(优先绘制的内容)
	void set_canvas_data1(
		std::vector<std::vector<QPointF>*>& pts1, std::vector<const PointStyle*>& ss_pts1,
		std::vector<std::vector<LineUI>*>& lines1, std::vector<const LineStyle*>& ss_lines1,
		std::vector<std::vector<LineUI>*>& polys1, std::vector<const PolygonStyle*>& ss_polys1,
		std::vector<std::vector<LineUI>*>& linePts1, std::vector<const PointStyle*>& ss_linePts1,
		std::vector<std::vector<CircleUI>*>& circle1, std::vector<const PolygonStyle*>& ss_circle1
	) override;

	// 2.设置前景绘图信息(最后绘制的内容)
	void set_canvas_data2(
		std::vector<std::vector<QPointF>*>& pts2, std::vector<const PointStyle*>& ss_pts2,
		std::vector<std::vector<LineUI>*>& lines2, std::vector<const LineStyle*>& ss_lines2,
		std::vector<std::vector<LineUI>*>& polys2, std::vector<const PolygonStyle*>& ss_polys2,
		std::vector<std::vector<LineUI>*>& linePts2, std::vector<const PointStyle*>& ss_linePts2,
		std::vector<std::vector<CircleUI>*>& circle2, std::vector<const PolygonStyle*>& ss_circle2
	) override;

	// 3.设置动态绘图信息(下拉选择显示哪个数据)
	void set_canvas_data3(
		std::vector<std::vector<LineUI>*>& lines3, std::vector<const LineStyle*>& ss_lines3
	) override;

public:
	Style_dsn m_styleInfo = Style_dsn();

private:
	std::shared_ptr<db::dsn::Data_dsn> m_dbData;   // 关联内核数据
	mutable QPointF m_cachedMinPt;
	mutable QPointF m_cachedMaxPt;

	void syncBBoxCache() const {
		if (!m_dbData) return;
		const auto& b = m_dbData->m_bbox;
		if (!b.valid()) return;
		m_cachedMinPt = QPointF(b.minX(), b.minY());
		m_cachedMaxPt = QPointF(b.maxX(), b.maxY());
	}

	void setupStyleManager() override {
		m_styles.setPointStyle(m_styleInfo.pointStyleKeys, m_styleInfo.pointStyles);
		m_styles.setLineStyle(m_styleInfo.lineStyleKeys, m_styleInfo.lineStyles);
		m_styles.setPolygonStyle(m_styleInfo.polygonStyleKeys, m_styleInfo.polygonStyles);
	}
};
