#pragma once
#include "src_paint/DiagramStyle.h"
#include <QPointF>

// 前端数据基类：仅保存前端/绘图相关的数据（绘图边距 + 样式管理器）。
// 内核原始数据保存在 db::DataBase 子类中（通过 shared_ptr 在子类中关联）。
class DataBase {
public:
	DataBase() {}
	~DataBase() {}
public:
	// 前端绘图边距：Diagram 初始化视图变换时使用
	QPointF margin = QPointF(20, 20);

//protected:
	DiagramStyleManager m_styles;

public:
	void clear() {
		margin = QPointF(20, 20);
	};
};
