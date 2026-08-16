#pragma once
#include "src_data/dataStructUI.h"
#include "src_paint/DiagramStyle.h"

//数据结构基类：储存前端与后端用到的公共数据。具体文件格式的数据结构继承此类（见 src_data/dsn/）
class DataBase {
public:
	DataBase() {}
	~DataBase() {}
public:
	//1.边界数据
	QPointF margin = QPointF(20, 20);	//与坐标轴保持的边距
	QPointF minPt = QPointF(-2, -2);	//图形中最小的xy坐标
	QPointF maxPt = QPointF(-1, -1);    //图形中最大的xy坐标
//protected:
	DiagramStyleManager m_styles;

public:
	void clear() {
		margin = QPointF(20, 20);		//与坐标轴保持的边距
		minPt = QPointF(-2, -2);	    //图形中最小的xy坐标
		maxPt = QPointF(-1, -1);        //图形中最大的xy坐标
	};
};
