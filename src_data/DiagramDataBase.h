#pragma once
#include "src_data/dataStructUI.h"
#include "src_paint/DiagramStyle.h"
#include <memory>
#include <vector>

// 绘图数据接口基类 - 所有需要绘图的Data类都要实现这个接口
class DiagramDataBase {
public:
	virtual ~DiagramDataBase() = default;

	// 数据边界
	virtual QPointF* getMinPoint() = 0;
	virtual QPointF* getMaxPoint() = 0;

	// 1.设置画布信息(优先绘制的内容)
	virtual void set_canvas_data1(
		std::vector<std::vector<QPointF>*>& pts1, std::vector<const PointStyle*>& ss_pts1,
		std::vector<std::vector<LineUI>*>& lines1, std::vector<const LineStyle*>& ss_lines1,
		std::vector<std::vector<LineUI>*>& polys1, std::vector<const PolygonStyle*>& ss_polys1,
		std::vector<std::vector<LineUI>*>& linePts1, std::vector<const PointStyle*>& ss_linePts1,
		std::vector<std::vector<CircleUI>*>& circle1, std::vector<const PolygonStyle*>& ss_circle1
	) = 0;
	// 2.设置背景绘图信息(第二优先级绘制的内容)
	virtual void set_canvas_data2(
		std::vector<std::vector<QPointF>*>& pts2, std::vector<const PointStyle*>& ss_pts2,
		std::vector<std::vector<LineUI>*>& lines2, std::vector<const LineStyle*>& ss_lines2,
		std::vector<std::vector<LineUI>*>& polys2, std::vector<const PolygonStyle*>& ss_polys2,
		std::vector<std::vector<LineUI>*>& linePts2, std::vector<const PointStyle*>& ss_linePts2,
		std::vector<std::vector<CircleUI>*>& circle2, std::vector<const PolygonStyle*>& ss_circle2
	) = 0;
	virtual void set_canvas_data3(
		std::vector<std::vector<LineUI>*>& lines3, std::vector<const LineStyle*>& ss_lines3
	) = 0;
protected:
	virtual void setupStyleManager() = 0;
};
