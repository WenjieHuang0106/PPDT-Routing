#pragma once

#include<math.h>
#include<iostream>
#include <algorithm>
#include <optional>
#include <iomanip>
#include <fstream>
#include <unordered_set>

#include "dataStructAlg.h"




// Utils中的函数
double norm(Point a);
double norm(const Line& a);
double cross(Point a, Point b);											// 叉乘

double backMinDistNum(std::vector<double> DistNums);
Point getProjectionPoint(Point basePoint, Line baseLine);				// 求出投影点的位置
Point RotateVec(Point vec, double angle);								// 返回向量旋转后的角度 || > 0 --> 逆时针
Point getInsection(Point Vec1, Point p1, Point Vec2, Point p2);			// 已知两条线段的方向向量和线段上一点求两条直线的交点，求交点
Point rotate(Point vec, double angle);                                  // 向量旋转,[1] > 0:逆时针
double CrossVecs(Point a, Point b);                                     // 两个向量的叉乘
void NormalizeVec(Point& NormalVec);                                    // 向量单位化
double pointToLineDist(Point point, Line line1);						// 点到直线的距离
double linearDist(Point point1, Point point2);							// 得到两点之间的直线距离

namespace v2Hash {
	struct PointHash {
		size_t operator()(const Point& pnt)const;
	};
	struct LineHash {
		size_t operator()(const Line& line)const;
	};
}


// Hug
struct PointHash {
	size_t operator()(const Point& data) const {
		return std::hash<double>()(data.x) ^ std::hash<double>()(data.y); //^ std::hash<double>()(data.Pt2.x);
	}
};

bool CheckSegmentAngle(const Line& InputSegment);								 // 判断走线角度是否符合 135°要求
Point CountLineMidPos(const Point& Point1, const Point& Point2);                 // 返回两个点连线的中点
Point CountLineMidPos(const Line& BaseSegment);									 // 返回一条线段的中点
double CountLineAngle(const Line& BaseSeg);										 // 返回一条线的角度（0° - 180°）
Point CountCrossingPoint(Point Vec1, Point p1, Point Vec2, Point p2);            // 给定两点以及该点所在的某个方向求交点
std::vector<Line> PointsToLines(std::vector<Point> points, bool flag);			 // 将一些点按照顺序连接成线
std::vector<Point> LinesToPoints(std::vector<Line> lines, bool flag);			 // 线->点
Point getCenterOfObstacles(const std::vector<std::vector<Line> >& InputObstacles);                 // 获取一组障碍物的中心点
bool CheckNumInSets(const double& InputNumber, const std::vector<double>& InputNumSets);           // 判断InputNumber是否在InputNumSets数字集合中
bool CheckNumInSets(const int& InputNumber, const std::vector<int>& InputNumSets);				   // 判断InputNumber是否在InputNumSets数字集合中
int getPointPosition(const std::vector<Point>& pointsVec, const Point& basePoint);			// 获取当前点在点集中的位置
bool isEncloseGraph(std::vector<Line> lines);												// 判断输入的一组线是障碍物还是线
bool pointInPointsVec(Point point, std::vector<Point> points);								// 判断一个点是否在一个点集中
void pointsToNewVec(std::vector<Point> points_, std::vector<Point>& newPointsVec);          // 将Points中的点全部添加到newPointsVec中去
std::vector<Line> linkToLine(Point sp_, Point ep_);											// 将两点直接连成线
Point getMidpoint(Point point1, Point point2);                                              // 求中点
Point getMidpoint(const Line& line);








