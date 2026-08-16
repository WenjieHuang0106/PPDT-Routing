#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "dataStructAlg.h"
#include "../src_dsn/RoutingNode.h"

// 路由器抽象基类（Qt-free）。
// 接口签名对接 RouterMeshless 现有公有方法；routerReset 由原引用传递改为值传递 gridSize。
class RouterBase {
public:
	virtual ~RouterBase() = default;
	virtual void run(std::vector<std::string>& routingInfo) = 0;
	virtual void routerReset(double gridSize) = 0;
	virtual bool checkBeforPush(PathNode* node, PolyShape* shape, PolyShape* shapeTmp, const Point& pushVec, bool setPath) = 0;
	virtual void pushLineDataUpdate(const PolyShape* shapeCopy, PolyShape* pathShape) = 0;
	// 配置类方法
	virtual void setDirectionOpt(int directionOp) = 0;
	virtual void setRouterOption(const std::vector<bool>& boolOps) = 0;
	virtual void setDebugOpt(const std::string& flexibleOpt, bool breakFunOn, const Point& pt, int breakID) = 0;
	virtual void setPushTimes(int pushTimes) = 0;
	// 输出获取
	virtual std::vector<PathTree*>* getTreesHeadsOrdered() = 0;
	virtual std::unordered_map<PathNode*, PolyShape>* getPaths() = 0;
	virtual std::unordered_set<Point, Point::Hash>* getPlanningPts() = 0;
	virtual std::unordered_map<Point, PinPad, Point::Hash>* getVias() = 0;
};
