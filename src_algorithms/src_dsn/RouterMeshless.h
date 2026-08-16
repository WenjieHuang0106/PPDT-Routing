#pragma once
#include "MST.h"
#include "Grid.h"
#include "RoutingNode.h"
#include <stack>
#include <map>

//多线程
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

class RouterMeshless {
public:
	RouterMeshless()
		:m_netTrees(nullptr), m_pads(nullptr), m_preVias(nullptr), m_viaInfos(nullptr), m_netsInfos(nullptr), m_nets(nullptr) {
	};
	RouterMeshless
	(
		unordered_map<string, STN>* netTrees,
		unordered_map<string, PinPad>* pads,
		unordered_map<string, PinPad>* preVias,
		vector<double>* bound,
		unordered_map<string, ViaInfo>* netViaInfos,
		unordered_map<string, NetInfo>* netInfos,
		unordered_map<string, vector<PinPad*>>* nets
	)
		: m_netTrees(netTrees), m_pads(pads), m_preVias(preVias), m_bound(*bound),
		m_viaInfos(netViaInfos), m_netsInfos(netInfos), m_nets(nets)
	{
		m_steinerQuery.clear();
		if (m_netTrees) {
			for (const auto& [netName, root] : *m_netTrees) {
				buildSteinerQuery(root);
			}
		}
		m_worker = std::thread(&RouterMeshless::workerLoop, this);
	};
	~RouterMeshless() {
		m_stop = true;
		m_cv.notify_all();
		if (m_worker.joinable())
			m_worker.join();
		freePathsAndTrees();
	};
	void run(vector<string>& routingInfo);
	void routerReset(double& gridSize);

	void pushAndUpdate(PathNode* pushNode, const Point& pushVec, PolyShape* shape);

	void pushMoveLine(PathNode* node, const Point& offset, PolyShape* shape = nullptr);
	bool checkBeforPush(PathNode* node, PolyShape* shape, PolyShape* shapeTmp, const Point& pushVec, bool setPath);
	bool copyAndCheckBeforPush(PathNode* pushNode, const Point& pushVec, PolyShape* shape);
	bool pushMoveLineTmpToCheck(PathNode* nodeInputLock, const Point& offset, PolyShape* shapeTmp);


	void pushLineDataUpdate(const PolyShape* shapeCopy, PolyShape* pathShape);
	void addOnePathToGrid(PolyShape* shape) {
		m_gridManager->addShapeLines(shape);
	}
	void removeOnePathFromGrid(PolyShape* shape) {
		m_gridManager->removeOnePath(shape);
	}
	void setDirectionOpt(const int& directionOp) { m_directionOp = directionOp; };
	void setGrideSizeFactor(const double& alpha_g) { m_grideSizeFactor = alpha_g; };
	void setStandardCostFactor(const double& beta) { m_standardCostFactor = beta; };
	void setDecayFactor(const double& eta) { m_decayFactor = eta; };
	void setRouterOption(const vector<bool>& boolOps) {
		size_t opNum = boolOps.size();
		if (opNum > 0) m_postOn = boolOps[0];
		if (opNum > 1) m_GNDRoute = boolOps[1];		// GND特殊线序
		if (opNum > 2) m_VCCRoute = boolOps[2];		// VCC特殊线序
		if (opNum > 3) m_DiffRoute = boolOps[3];
		if (opNum > 4) m_autoPush = boolOps[4];
		if(opNum > 5) m_4_8Tree = boolOps[5];
	};
	void setDebugOpt(const string& flexibleOpt, const bool& breakFunOn, const Point& pt, int breakID) {
		for (size_t i = 0; i < flexibleOpt.size(); ++i) {
			char c = flexibleOpt[i];
			bool value = (c != '0');  // 非'0'字符都为true，只有'0'为false
			switch (i) {
			case 0: m_pinPairExchange = value; break;	//1.引脚对换
			case 1: m_cutAcuteAngle = value; break;		//2.尖角裁剪
			case 2: m_pushOn = value; break;			//3.是否启用推挤避让
			case 3: m_fixWireSpacingOn = value; break;	//4.是否启用线间距修正
			case 4: m_GNDOutToGNDLayer = value; break;	//5.GND引脚输出到GND层
			case 5: m_GNDViaCountON = value; break;		//6.GND引脚via数
			}
		}
		m_debugFunOn = breakFunOn;
		m_beeakPt = pt;
		m_breakIndex = breakID;
	};
	void setPushTimes(int pushTimes) { m_pushTimes = pushTimes; };
	vector<PathTree*>* getTreesHeadsOrdered() { return &m_pathTreesOrdered; };
	unordered_map<PathNode*, PolyShape>* getPaths() { return &m_paths; };
	unordered_set<Point, Point::Hash>* getPlanningPts() { return &m_planningPts; };
	unordered_map<Point, PinPad, Point::Hash>* getVias() { return &m_vias; };

private:		//调试的数据
	// 1.1算法执行选项
	bool m_postOn = true;
	bool m_GNDRoute = true;
	bool m_VCCRoute = false;
	bool m_DiffRoute = false;
	bool m_autoPush = true;
	bool m_4_8Tree = true;
	bool m_mergeDiffPathLines = true;
	int m_directionOp = 0;				//0任意方向，1四方向，2八方向

	// 1.2算法执行选项，灵活定义
	bool m_pinPairExchange = true;		//1.引脚对换
	bool m_cutAcuteAngle = true;		//2.尖角裁剪
	bool m_pushOn = false;				//3.是否启用推挤避让
	bool m_fixWireSpacingOn = false;	//4.是否启用线间距修正
	bool m_GNDOutToGNDLayer = true;		//5.GND引脚输出到GND层
	bool m_GNDViaCountON = true;		//6.计算GND直接输出到CND层产生的引的via数


	//2.调试中断参数
	bool m_debugEnd = false;	// 中断布线，并返回已布线结果
	bool m_debugFunOn = false;
	Point m_beeakPt;
	int m_breakIndex = 0;
	bool debugBreak(const PathTree* const curNode) {
		return debugBreak(curNode->pos);
	}
	bool debugBreak(const Point& pos) {
		if (!m_debugFunOn)
			return false;
		if (m_breakIndex > 0 && m_breakIndex != m_curPathIndex)
			return false;
		double delta = 1;
		bool xInMid = pos.x > m_beeakPt.x - delta && pos.x < m_beeakPt.x + delta;
		bool yInMid = pos.y > m_beeakPt.y - delta && pos.y < m_beeakPt.y + delta;
		if (xInMid && yInMid) {
			cout << "debugBreak in path index:[ " << m_curPathIndex << " ]======================" << endl;
			m_debugEnd = true;
			return true;
		}
		else
			return false;
	}

private:
	// 1.算法输入数据
	unordered_map<string, STN>* m_netTrees;		// net_name -> 树根
	unordered_map<Point, STN, Point::Hash> m_steinerQuery;
	unordered_map<string, PinPad>* m_pads;			// pad_name(pin_name) -> pad(所有引脚或过孔对应的焊盘)
	unordered_map<string, PinPad>* m_preVias;		// pad_name(pin_name) -> pad
	vector<double> m_bound;							// xmin,ymin,xmax,ymax
	unordered_map<string, ViaInfo>* m_viaInfos;		// via_name -> via
	unordered_map<string, NetInfo>* m_netsInfos;	// net_name -> via
	unordered_map<string, vector<PinPad*>>* m_nets;
	string m_NetNameGND = "GND";
	string m_netNameVCC_1 = "VCC";
	int m_layerGND = 0;

	// 2.算法超参数
	double m_grideSizeFactor = 2.5;				//alpha_g
	double m_standardCostFactor = 1.2;			//beta=0.5，拥塞与过孔代价因子，实验取值0.5，1.2
	double m_decayFactor = 0.3;					//eta=0.55,0.6，密度代价衰减系数(0,1)
	int m_rangeSize = 2;						//拥塞范围大小
	double m_test_max = -1;
	double m_test_min = 10;
	double m_miter = sqrt(2) * 2 - 2;	// 按直角切除长度计算得到临界值为 sqrt(2) * 2 - 2

	double rectLengthFactor = 10.5;				//长宽比大于该值，认为是长条形焊盘，只能沿着长条方向引出走线
	const double m_changeSTFactor = 0.15;		//beta_st
	double m_pathViaMinLength = 4;				//换层边的最小长度

	// 3.算法过程中不断补充的数据
	unique_ptr<GridManager> m_gridManager;				// 空间索引管理器
	unordered_map<Point, PinPad, Point::Hash> m_vias;	// 过孔
	unordered_map<PathTree*, string> m_pathHeads;		// 路径对应的网表名
	unordered_set<Point, Point::Hash> m_planningPts;	// 仅用于UI绘图
	unordered_map<const PathTree*, PinPad*> m_PinQuaryPad;	//起点终点节点对应的焊盘
	unordered_set<Point, Point::Hash> m_GNDConnected;		//已经接到GND层的引脚

	// 4.单条路径搜索使用的数据
	double m_standartCost = 0;			//起点终点距离 * m_standardCostFactor
	double m_viaRadius = 1;
	int m_serchTimesLimit = 1000;
	string m_curNetName;				//当前布线的net
	NetInfo* m_curNetInfo = nullptr;
	PathTree* m_node_start = nullptr;
	PinPad* m_startPad = nullptr;
	vector<STN> m_startNeibs;
	PathTree* m_node_end = nullptr;
	PinPad* m_endPad = nullptr;
	vector<STN> m_endNeibs;
	vector<Point> m_endExits;
	Point m_ptToEnd;
	double m_ptToEndCost = 0;
	unordered_set<int> m_routingLayers;	// 可布线的层
	unordered_set<int> m_startLayers;	//起点在哪些层
	unordered_set<int> m_endLayers;		//终点在哪些层
	int m_postTimes = 0;				//当前后处理递归的次数

	priority_queue<PathTree*, vector<PathTree*>, ComparePathTreePtr> m_cdNodesList;		//待扩展的叶子节点
	unordered_map<Point, unordered_map<int, PathTree*>, Point::Hash> m_exploredNodes;	//已经搜的规划点
	queue<PolyShape*> m_candidateObss;
	unordered_set<PolyShape*> m_queryObssPassed;
	unordered_set<Point, Point::Hash> m_querySTChanged;		//已经被修改过的起点终点（被pass掉的）

	// 5.统计数据（算法执行过程中复位或修改）
	int m_totalPins = 0;
	int m_curPathIndex = 0;		//当前正在搜索的路径序号
	int m_searchTimes = 0;
	int m_pathFoundNum = 0;	//统计数据
	int m_viasSum = 0;
	double m_totalPathLength = 0;
	int m_treeNodesSum = 0;
	int m_DRCCnt = 0;
	unordered_map<string, bool> m_netFound;
	unordered_map<PathNode*, double> m_pathLengths;

	// 6.算法执行结果
	unordered_map<string, vector<PathTree*>> m_treesHeads;	//net_name->所有路径, 树结构
	vector<PathTree*> m_pathTreesOrdered;					//搜索树根节点（有序）
	unordered_map<PathNode*, PolyShape> m_paths;			//路径起点对应的链表节点，该节点对应的路径（线段集合）
	unordered_map<string, vector<Line>> m_pathLines;		//几何重构之后的布线结果

private:
	function<bool(const pair<PinPad*, PinPad*>&, const pair<PinPad*, PinPad*>&)> m_priorityRule;
	// 默认优先级规则：按距离排序（距离短的优先）
	static bool defaultPriorityRule(const pair<PinPad*, PinPad*>& a, const pair<PinPad*, PinPad*>& b) {
		double distA = a.first->pos.distanceTo(a.second->pos);
		double distB = b.first->pos.distanceTo(b.second->pos);
		// 首先比较距离
		if (std::abs(distA - distB) > MapMinValue) {  // 考虑浮点数精度
			return distA < distB;
		}
		double a_minX = std::min(a.first->pos.x, a.second->pos.x);
		double b_minX = std::min(b.first->pos.x, b.second->pos.x);
		if (std::abs(a_minX - b_minX) > MapMinValue) {
			return a_minX < b_minX;
		}
		double a_minY = std::min(a.first->pos.y, a.second->pos.y);
		double b_minY = std::min(b.first->pos.y, b.second->pos.y);
		if (std::abs(a_minY - b_minY) > MapMinValue) {
			return a_minY < b_minY;
		}
		return distA < distB;
	};
	void buildSteinerQuery(const STN& root) {
		if (!root) return;

		stack<STN> stk;
		stk.push(root);

		while (!stk.empty()) {
			auto node = stk.top();
			stk.pop();
			m_steinerQuery.emplace(node->position, node);
			for (const auto& child : node->children) {
				if (child) {
					stk.push(child);
				}
			}
		}
	}

private:
	//数据准备与变更
	void freePathsAndTrees();
	void initializeGrid(const double& gridSize);      // 初始化网格
	void setSpecialNetInfomation(string netName);
	void extractPinPairs(const STN& steinerTree, vector<pair<PinPad*, PinPad*>>& pinPairs);
	void collectSubtree(STN S, STN T, vector<STN>& S_sub, vector<STN>& T_sub);
	void setOrderedPinPair(PinPad* pin1, PinPad* pin2, vector<pair<PinPad*, PinPad*>>& pinPairs);
	bool getPlanningPos(PathNode* node, Point& poss, bool isLine);
	void mergeNode(PathNode* node);
	void findAllPaths(vector<pair<PinPad*, PinPad*>>& pinPairs);
	void findGNDPaths(vector<pair<PinPad*, PinPad*>>& pinPairs);
	bool prepareOnePathData(pair<PinPad*, PinPad*>& pair);
	void setStartAndEnd();
	void setStartPad(int layer_s);
	void setEndPad(int layer_e);
	void setPadOuPts(PathTree* nodeToSet, PathTree* nodeToIgnore, double minClear, const vector<double>& box, int startIdx, int step, vector<Point>& outDirectionsPts);
	int getPinPadOutDirection(PathTree* const nodeSE, PinPad*& padptr);
	bool changeStartOrEnd(bool isStart, PinPad* pad, int layer);
	double getPathsLength();
	double getOnePathLength(PathNode* head);
	PinPad* getPadPtr(const string& pinName) {
		if (m_pads->contains(pinName))
			return &m_pads->at(pinName);
		else if (m_preVias->contains(pinName))
			return &m_preVias->at(pinName);
		else
			return nullptr;
	};
	int getSpecialRoutingType();
	bool needToRoute();



	//PPDT布线算法
	bool runPPDT();
	bool route_GND();
	bool isTreeGNDConnected(PathTree* node);
	PathTree* nodeSelection();
	void obssExploration(const PathTree* start);
	void getCandidateObss(const unordered_set<PolyShape*>& obss, const Line& line);
	void getValidObssInCells(const vector<GridCell*>& cells, const Line& line, int layer, unordered_set<PolyShape*>& obss);
	bool nodeExpansion(PathTree* start);
	void nodeExpansionWithObsShape(PathTree* start, PolyShape* obsShape);
	bool checkConnectToEnd(PathTree* start);
	int getBlockTypeToEndNode(PolyShape* shape, PathTree* start)const;
	void setNeibNodeAsChild(PathTree* start, PolyShape* shape);
	void setAllPtsAsChildren(PathTree* start, PolyShape* currentPad);
	bool connectToPos(PathTree* start, const Point& pos, PathNode* vertexNode, bool inserVia);
	PathTree* posGetNode(const Point& pos, int layer);
	bool cmpStepCost(PathTree* start, const Point& pos, int layer, double& G, double& H, double& E);
	void updateExistingNode(PathTree* start, PathTree* oldNode, const double& G, const double& H, const double& E);
	PathTree* addOneChild(PathTree* start, const Point& pos, int layer, PathNode* vertexNode, bool allowVia);
	double computeSmoothness(const Point& prev, const Point& cur, const Point& next);
	bool isStandardDirection(const Point& p1, const Point& p2)const;

	//4/8方向搜素树
	void reconstructDirection(PathTree* u, PathTree* v, int depth);
	double getCongestionValue(const Point& pos, double r, int layer1, int layer2);//const
	int getCongestionSum(const Point& pos, double r, int layer1, int layer2) const;
	PolyShape* getFirstShape2(const Point& p1, const Point& p2, int layer, PinPad* ignorePad2 = nullptr);
	bool isReachable2(const Point& p1, const Point& p2, int layer, PinPad* ignorePad2, PolyShape** firstObsPtr = nullptr);

	PolyShape* getFirstShape(const Point& p1, const Point& p2, int layer, PinPad* ignorePad, PinPad* ignorePad2 = nullptr);
	bool isReachable(const Point& p1, const Point& p2, int layer, PinPad* ignorePad1, PinPad* ignorePad2, PolyShape** firstObsPtr = nullptr);
	bool tryToPushAndReach(const Point& p1, const Point& p2, int layer, PolyShape* obs);
	bool pushToAvoid(const Point& p1, const Point& p2, double width, double clear, PolyShape* path);

	bool checkPushLine(PathNode* M, PathNode* N, const Point pushVec);

	void setSEViasNode(PathNode* head, PathNode* tail);
	bool insertVias(PathNode* head, PathNode* tail);
	bool insertOneVia(PathNode* viaPre);
	bool checkViaPos(const Point& pos, int layer1, int layer2);
	bool pushViaAndLine(PathNode* nodePre, int layer1, int layer2);
	bool moveSEVia(PathNode* viaPre, int layer1, int layer2, bool isStart);
	bool backTrackOnePath(PathTree* nodeEnd);

	//后处理
	const vector<int> shape_out_direction = { 1, 2, 2 };  // 圆，长方形，接近正方形，1八个方向，2四个方向，4两个方向
	const vector<Point> Direction8 = { Point(1,0),Point(sqrt(2) / 2,sqrt(2) / 2),		//0x，1现有，2y...
			Point(0,1), Point(-sqrt(2) / 2,sqrt(2) / 2), Point(-1,0),
			Point(-sqrt(2) / 2,-sqrt(2) / 2), Point(0,-1),
			Point(sqrt(2) / 2,-sqrt(2) / 2) };  //8个标准方向
	void checkNewST(PathTree* curStart, bool changeEnd, map<double, PinPad*>& newPads);
	void checkNewStartNode();						//1.引脚切换（回溯过程）
	void fixWireSpacing(PathNode* head);
	void getPostPtsToPass(const Point& p1, const Point& p2, int layer1, int layer2, Point& ptToBypass);
	bool lineMidPoss_4d(const Line& line, Point& midPos1, Point& midPos2);
	bool lineMidPoss_8d(const Line& line, Point& midPos1, Point& midPos2);

	void cutAngle(PathNode* const head);					//2.尖角裁剪
	void cutAngle_ml(PathNode* const head);					//2.尖角裁剪
	bool cut90Angle(PathNode* cur, PathNode* A, PathNode* C, const Point& vec1, const Point& vec2)const;
	bool cut45Angle(PathNode* cur, PathNode* A, PathNode* C, const Point& vec1, const Point& vec2)const;
	void generateOnePath(PathNode* const head);

	//后处理方向约束
	void directionStandarlize(PathNode* head);
	void reconstructDirection(PathNode* start, PathNode* end, int depth);

	//推线算法
	Point lineIntersection(const Point& p1, const Point& p2, const Point& p3, const Point& p4);
	bool isParallel(const Point& v1, const Point& v2)const;
	bool isParallel(const Point& p1, const Point& p2, const Point& p3, const Point& p4)const;
	Point outCrosingPos(const Point& tPos1, const Point& tPos2, const Point& SE, bool istPos1);
	void removeVias(PathNode* node1, PathNode* node2);
	void resetVias(PathNode* node1, PathNode* node2);
	void getConnectionPoints(PathNode* M, PathNode* N, PathNode*& D, PathNode*& E);
	void getMPrevNodeToChange(const double& projLength, const Point& normal, PathNode*& M);
	void getNNextNodeToChange(const double& projLength, const Point& normal, PathNode*& N);
	void setNodeConnections(PathNode* Node1, PathNode* Node2, const Point& tPos1, const Point& tPos2, PolyShape* shape);
	bool collisionCheck(const PathNode* M, const PathNode* N);
	void checkPathData(PathNode* head, PathNode* tail);

	//几何变换
	int m_pushTimes = 20;			//安全迭代次数
	double m_pushStepdecay = 0.9;	//自动推挤步长衰减系数
	double m_gamaL = 1;				//线密度系数
	double m_epsilon = 1;			//距离软化值，取0.5width
	double m_1_decay_exp20 = 1.0 - std::pow(m_pushStepdecay, m_pushTimes+1);	//避免重复计算
	unordered_map<PathNode*, Point> m_pushVecLock;
	struct NodeForceData {
		Point pushVec;      // 推挤方向单位向量
		double F_g;			//归一化引力
		NodeForceData() : F_g(0), pushVec(Point(0, 0)) {}
	};
	
private:
	string m_netNameBackup;
	void netNameChange(const string& netName) {
		m_netNameBackup = m_curNetName;
		m_curNetName = netName;
		m_curNetInfo = &m_netsInfos->at(m_curNetName);
	}
	void netNameReset() {
		m_curNetName = m_netNameBackup;
		m_curNetInfo = &m_netsInfos->at(m_curNetName);
	}

private:
	thread m_worker;
	queue<int> m_idxQueue;
	vector<int> m_treeIdxs;
	mutex m_mutex;
	condition_variable m_cv;
	atomic<bool> m_stop{ false };		// 请求停止并结束线程
	atomic<bool> m_pause{ false };     // 请求暂停
	atomic<bool> m_working{ false };   // worker 是否正在 cut

	void workerLoop();
	void cut(int idx);
	//m_pathTreesOrdered
	//m_paths
	//操作共享变量的方法：
	//m_pause = true;
	//操作数据
	//m_pause = false;
	//m_cv.notify_one();

	//几何重构
	double getMergedPathsLength();
	unordered_map<string, vector<Line>> collectNetLinesFromPaths() const;
	bool areSameInfiniteLine(const Line& l1, const Line& l2) const;
	vector<vector<Line>> groupCollinearLines(const vector<Line>& lines) const;
	vector<Line> atomizeAndMergeCollinearGroup(const vector<Line>& group) const;
	bool segmentCoveredByAnyLine(const Point& a, const Point& b, const vector<Line>& linesOnSameLine) const;
	double pointParamOnLine(const Point& origin, const Point& dirUnit, const Point& p) const;
};


