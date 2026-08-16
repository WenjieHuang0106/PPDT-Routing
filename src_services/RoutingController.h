#pragma once
#include "../src_data/dsn/Data_dsn.h"
#include "../src_config/AppConfig.h"
#include "../src_algorithms/src_dsn/MST.h"  // 层感知斯坦纳树
#include "../src_algorithms/src_dsn/RouterMeshless.h"				// PDT算法
#include <string>

using namespace std;

class RoutingController {
public:
	RoutingController(Data_dsn* data, AppConfig* config)
		:m_data(data), m_config(config) {
	}
	~RoutingController() {
		//1.释放布线器
		if (m_router) {
			delete m_router;
			m_router = nullptr;
		}
	}

	// 1.1 寻路部分
	void routingRunBegin();
	void setNetMST();
	void routingRun(const QString& qFileName = "");

private:
	//读取的数据
	Data_dsn* m_data;
	AppConfig* m_config;
	SteinerTreeSolver m_solver;
	RouterMeshless* m_router = nullptr;
	double m_totalLenOfRoutedDsn = 0;

	

	//算法输入数据（算法初始化之前从m_data中生成）
	unordered_map<string, shared_ptr<SteinerNode>> m_netTrees;		// 连接关系树
	unordered_map<string, PinPad> m_pads;			// pad_name(pin_name) -> pad
	unordered_map<string, PinPad> m_preVias;		// pad_name(pin_name) -> pad
	unordered_map<string, PinPad> m_RSTNodePasd;	// pad_name(pin_name) -> pad
	vector<double> m_bound;
	unordered_map<string, ViaInfo> m_viaInfos;		// via_name -> via
	unordered_map<string, NetInfo> m_netsInfos;		// net_name -> 网表各种约束信息
	unordered_map<string, vector<PinPad*>> m_nets;

	//算法执行结果
	vector<vector<Line>> m_flyLines;
	vector<PathTree*>* m_treesHeads = nullptr;
	unordered_map<PathNode*, PolyShape>* m_pathsShapes = nullptr;
	unordered_set<Point, Point::Hash>* m_planningPts = nullptr;
	unordered_map<Point, PinPad, Point::Hash>* m_vias = nullptr;

private:
	void experment0(QTextStream* ofs);

	void dataInit();
	void fillPaintFlyLines();
	void fillPaintPathLines();
	void addPreViasToUi(vector<CircleUI>& ui_vias);
	void fillPaintTrees();
	void fillPlanningPt();

public:
	bool pushLineRun(const QPointF& offset, const bool inGreed, bool algUpdate);
	void pushLineRunBegin(LineUI& line, vector<LineUI>& lines);			//设置算法数据，算法执行的起点
	bool pushPinRun(const QPointF& offset, bool inGreed = false) { return true; };
	void pushPinRunBegin(QPointF pt, vector<LineUI>& lines) {};			//设置算法数据，算法执行的起点
private:
	PathNode* m_selectPathPreNode = nullptr;
	QPointF m_selectPt;
	PolyShape* m_selectShape = nullptr;
	PolyShape* m_shapeSuccessed = nullptr;			//无冲突推挤的结果（用于Block模式）
	vector<LineUI>* m_ui_wires = nullptr;
	bool setPushLineData(PolyShape* shapeInput, PathNode*& nodeInput, Point& offsetInput, const QPointF& offsetUI);
	void pushLineUIDataUpdate(PolyShape* shape, bool algUpdate);			//算法执行结束，更新前端数据
	bool setPushPinData(vector<Line>& polyLines_input, Point& pt_input, Point& offset_input, const QPointF& offset_ui) { return true; };

};