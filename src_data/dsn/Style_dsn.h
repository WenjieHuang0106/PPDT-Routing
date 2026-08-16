#pragma once
#include "../../src_paint/DiagramStyle.h"
#include <vector>

//选中标记说明：m_type缺省为0
// 0不可选中
// 1-999完全可选中
// 1001及以上为线段只能选中线段
// 2001及以上为线段只能选中端点
// 6001: 仅允许悬浮捕捉并显示，不允许移动操作
struct Style_dsn {
	// 1.定义点样式
	QString planningPtKey = "planningPt";
	QString viaKey = "viaCenter";
	QString ptPadKey = "ptPadCenter";
	std::vector<QString> pointStyleKeys = { planningPtKey, viaKey, ptPadKey };
	std::vector<PointStyle> pointStyles = {
		// 轮廓颜色，显示轮廓，轮廓宽度，填充，填充颜色，半径，选中标记
		PointStyle(Qt::black,true,1.0,true,Qt::gray,2.0,6001),	// 规划点
		PointStyle(Qt::black,true,1.0,true,Qt::blue,5.0,1),		// 过孔中心点
		PointStyle(Qt::black,false,1.0,true,Qt::red,2.0,6001),	// 焊盘中心点
	};
	// 2.定义线样式
	QString flyLineKey = "flyLine";
	QString treeLineKey = "treeLine";
	std::vector<QString> pathLineKeys = { "pathLine0","pathLine1","pathLine2","pathLine3" };
	std::vector<QString> lineStyleKeys = { flyLineKey, treeLineKey, pathLineKeys[0],pathLineKeys[1] ,pathLineKeys[2] ,pathLineKeys[3] };
	std::vector<LineStyle> lineStyles = {
		// 颜色，宽度，线形，选中标记
		LineStyle(Qt::blue, 1.0, Qt::SolidLine,6001),		//飞线样式
		LineStyle(Qt::gray, 2.0, Qt::SolidLine,0),			//树样式

		LineStyle(Qt::red, 3.0, Qt::SolidLine,1),		//pathLine0
		LineStyle(Qt::blue, 3.0, Qt::SolidLine,1),		//pathLine1
		LineStyle(Qt::black, 3.0, Qt::SolidLine,1),		//pathLine2
		LineStyle(Qt::green, 3.0, Qt::SolidLine,1)		//pathLine3
	};
	QString getPathLineKey(int i) const { return pathLineKeys[i % 4]; };

	// 3.定义多边形或圆，样式
	float polygonPaintWidth = 2;		//多边形绘制宽度
	QString boundaryPolyKey = "boundaryPolygon";
	QString viaCicleKey = "viaCicleKey";
	std::vector<QString> padPolyKeys = { "padPolygon0","padPolygon1","padPolygon2","padPolygon3" };
	std::vector<QString> polygonStyleKeys = { boundaryPolyKey,viaCicleKey,padPolyKeys[0],padPolyKeys[1] ,padPolyKeys[2] ,padPolyKeys[3] };
	std::vector<PolygonStyle> polygonStyles = {
		// 外框颜色，是否外框可见，外框宽度，是否填充，填充颜色，选中标记
		PolygonStyle(Qt::black, true, 4.0,false,Qt::black,0),			//边框
		PolygonStyle(Qt::green, true, 2.0,true,Qt::green,0),			//过孔

		PolygonStyle(Qt::darkRed, true, polygonPaintWidth,false,Qt::gray,6001),		//padPolygon0
		PolygonStyle(Qt::darkBlue, true, polygonPaintWidth,false,Qt::gray,6001),		//padPolygon1
		PolygonStyle(Qt::darkYellow, true, polygonPaintWidth,false,Qt::gray,6001),	//padPolygon2
		PolygonStyle(Qt::darkGreen, true, polygonPaintWidth,false,Qt::gray,6001)		//padPolygon3
	};
	QString getPadPolyKey(int i) const { return padPolyKeys[i % 4]; };
};
