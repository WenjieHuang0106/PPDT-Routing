#include "RouterMeshless.h"
#include <algorithm>
#include <utility>
#include "../src_basics/utils.h"
#include <cstdio>
#include <map>


using namespace std;

//用于几何重构的结构体
struct EdgeKey {  //AB与BA是同一条线
	Point a;
	Point b;   // 规范化后：a <= b
	EdgeKey() = default;
	EdgeKey(const Point& p1, const Point& p2) {
		if (p2 < p1) {
			a = p2;
			b = p1;
		}
		else {
			a = p1;
			b = p2;
		}
	}
	bool operator==(const EdgeKey& other) const {
		return a == other.a && b == other.b;
	}
};
struct EdgeKeyHash {
	size_t operator()(const EdgeKey& e) const {
		Point::Hash pointHasher;
		size_t h1 = pointHasher(e.a);
		size_t h2 = pointHasher(e.b);
		return h1 ^ (h2 << 1);
	}
};

void RouterMeshless::routerReset(double& gridSize) {
	//1.算法过程中不断补充的数据
	m_debugEnd = false;
	m_vias.clear();
	m_pathHeads.clear();
	m_planningPts.clear();
	m_PinQuaryPad.clear();
	m_GNDConnected.clear();
	m_netFound.clear();
	m_layerGND = 0;
	m_searchTimes = 0;
	m_viasSum = 0;
	m_totalPathLength = 0;
	m_treeNodesSum = 0;

	//2.算法执行的结果数据
	freePathsAndTrees();

	//3.建立空间索引
	if (gridSize < 1) {
		double totalPadArea = 0.0;
		m_totalPins = 0;
		if (m_pads) {
			for (const auto& [padName, pad] : *m_pads) {
				const auto& box = pad.box;
				if (box.size() < 4) {
					cout << "Error: Invalid pad box size" << endl;
					continue;
				}
				double w = box[2] - box[0];
				double h = box[3] - box[1];
				if (w > 0 && h > 0) {
					totalPadArea += w * h;
					m_totalPins++;
				}
			}
		}
		if (m_totalPins > 0) {
			gridSize = m_grideSizeFactor * sqrt(totalPadArea / m_totalPins);
		}
		else {
			gridSize = 10.0;
		}
	}
	initializeGrid(gridSize);

	//2.网表布通信息初始化
	for (const auto& [netName, tree] : *m_netTrees) {
		m_netFound[netName] = true;
	}
}

void RouterMeshless::run(vector<string>& routingInfo) {
	if (m_netTrees == nullptr) {
		cerr << "Error: RouterMeshless::run() m_netTrees == nullptr" << endl;
		return;
	}
	if (!m_priorityRule)
		m_priorityRule = defaultPriorityRule;

	// 1.将steinerTree拆解成多组起点终点，并设置默认优先级规则
	vector<pair<PinPad*, PinPad*>> allPinPairs;
	for (const auto& [netName, steinerTree] : *m_netTrees) {
		if (!steinerTree) continue;
		extractPinPairs(steinerTree, allPinPairs);
		//设置特殊网表信息，如GND,VCC等(暂未实现VCC)
		setSpecialNetInfomation(netName);
	}
	// 2.对起点终点对进行排序
	sort(allPinPairs.begin(), allPinPairs.end(), m_priorityRule);
	// 3.算法数据重置
	size_t pathSum = allPinPairs.size();
	size_t netSum = m_nets->size();
	m_treesHeads.reserve(pathSum);	//预分配内存
	m_treeIdxs.reserve(pathSum * 2);
	m_curPathIndex = 0;
	m_pathFoundNum = 0;
	m_DRCCnt = 0;
	m_serchTimesLimit = pathSum * 7;
	if (m_serchTimesLimit < 2000)
		m_serchTimesLimit = 2000;
	// 4.提取特殊网表如GND,VCC等(暂未实现VCC)
	unordered_map<string, vector<pair<PinPad*, PinPad*>>> pinPairsToRoute;
	for (const pair<PinPad*, PinPad*>& pair : allPinPairs) {
		if (pair.first->netName == m_NetNameGND) {
			pinPairsToRoute[m_NetNameGND].emplace_back(pair.first, pair.second);
		}
		else {
			pinPairsToRoute["nets"].emplace_back(pair.first, pair.second);
		}
	}
	// 5.对所有起点终点对进行布线
	cout << "============================ Start routing: " << pathSum << " ============================" << endl;	//输出注释cout
	if (m_GNDRoute) {
		// 5.1 先对常规网表进行布线
		findAllPaths(pinPairsToRoute["nets"]);	//其他nets布线
		if (m_debugEnd)		// 调试中断布线
			return;
		//5.2 对GND网表进行布线
		if (m_GNDRoute) {		// GND特殊布线
			findGNDPaths(pinPairsToRoute[m_NetNameGND]);
		}
	}
	else {
		//不区分网表的布线方式，只区分优先级
		findAllPaths(pinPairsToRoute["nets"]);	//其他nets布线
		findAllPaths(pinPairsToRoute[m_NetNameGND]);
	}
	// 6.计算路径长度
	m_totalPathLength = getPathsLength();
	// 7.共线合并与几何变换(需要使用第6步中的长度来排优先级)
	
	// 8.计算布线长度
	if (m_mergeDiffPathLines) {
		m_totalPathLength = getMergedPathsLength();
	}
	// 调试相关内容
	int netFoundNum = 0;
	for (const auto& [netName, found] : m_netFound) {
		if (found) netFoundNum++;
	}
	double ratePinPairs = pathSum == 0 ? 0.0 : static_cast<double>(m_pathFoundNum) / pathSum * 100.0;
	double rateNets = m_netFound.empty() ? 0.0 : static_cast<double>(netFoundNum) / m_netFound.size() * 100.0;
	int layersSum = static_cast<int>(m_routingLayers.size());
	double avgLen = m_totalPathLength / max(m_pathFoundNum, 1);
	cout << "--------------------------->Summary:" << endl;
	cout << "layers:" << layersSum
		<< ", pairs:" << pathSum
		<< ", found:" << m_pathFoundNum
		<< ", failed:" << pathSum - m_pathFoundNum
		<< ", ratePair:" << ratePinPairs << "%"
		<< ", avgLen:" << m_totalPathLength / max(m_pathFoundNum, 1)
		<< ", TNodeSum:" << m_treeNodesSum
		<< ", via:" << m_viasSum
		<< ", DRCCnt:" << m_DRCCnt
		<< ", rateNet:" << fixed << setprecision(2) << rateNets << "%" << endl;
	cout << "\n============================ Routing Ended =================================" << endl;
	routingInfo.clear();
	char buf[32];
	routingInfo.emplace_back(to_string(layersSum));
	routingInfo.emplace_back(to_string(pathSum));
	routingInfo.emplace_back(to_string(m_pathFoundNum));
	routingInfo.emplace_back(to_string(pathSum - m_pathFoundNum));

	snprintf(buf, sizeof(buf), "%.2f%%", ratePinPairs);
	routingInfo.emplace_back(buf);

	snprintf(buf, sizeof(buf), "%.2f%", avgLen);
	routingInfo.emplace_back(buf);

	snprintf(buf, sizeof(buf), "%d", m_treeNodesSum);
	routingInfo.emplace_back(buf);

	routingInfo.emplace_back(to_string(m_viasSum));	// via

	routingInfo.emplace_back(to_string(m_DRCCnt));	//DEC

	snprintf(buf, sizeof(buf), "%.2f%%", rateNets);
	routingInfo.emplace_back(buf);

}
void RouterMeshless::initializeGrid(const double& gridSize) {
	if (m_bound.size() < 4) {
		cerr << "Error: Invalid boundary data. Expected 4 values: {minX, minY, maxX, maxY}" << endl;
		return;
	}
	// 检查nets是否为空
	if (!m_netTrees || m_netTrees->empty()) {
		cout << "Warning: No net trees to process" << endl;
		return;
	}
	double minX = m_bound[0];
	double minY = m_bound[1];
	double maxX = m_bound[2];
	double maxY = m_bound[3];
	if (minX >= maxX || minY >= maxY) {
		cerr << "Error: Invalid boundary range in initializeGrid" << endl;
		return;
	}
	// 1.创建网格管理器
	m_gridManager = make_unique<GridManager>(minX, minY, maxX, maxY, gridSize);
	// 2.所有引脚对应的多边形焊盘添加到网格中
	int totalPinPads = 0;
	for (auto& [padName, pad] : *m_pads) {
		m_gridManager->addPinPad(&pad);
		totalPinPads++;
	}
	// 3.所有预过孔对应的多边形焊盘添加到网格中
	for (auto& [padName, pad] : *m_preVias) {
		m_gridManager->addPinPad(&pad);
		totalPinPads++;
	}
	// 4.设置初始可布线层
	for (const auto& [shapeName, pad] : *m_pads) {
		for (const auto& [layer, shape] : pad.shapes) {
			if (!m_routingLayers.contains(layer))
				m_routingLayers.insert(layer);
		}
	}
	/*
	// 输出网格统计信息（用于调试）
	if (m_gridManager) {
		auto& allCells = m_gridManager->getAllCells();
		int nonPadEmptyCells = 0;
		int maxPadsInCells = 0;
		for (auto& cell : allCells) {
			int padsCount = cell->getPinPads().size();
			if (padsCount > 0) {
				nonPadEmptyCells++;
				maxPadsInCells = max(maxPadsInCells, padsCount);
			}
		}
		cout << "================Grid statistics================" << endl;
		cout << "totalPinPads" << totalPinPads << endl;
		cout << "\tnon-pad empty cells: " << nonPadEmptyCells << endl;
		cout << "\tmax pads in cells: " << maxPadsInCells << endl;
		cout << "================Grid statistics================" << endl;
	}
	*/
}
void RouterMeshless::freePathsAndTrees() {
	for (auto& net : m_treesHeads) {
		for (auto& node : net.second) {
			node->remove();
		}
	}
	m_treesHeads.clear();
	m_pathTreesOrdered.clear();
	// 释放内存m_paths
	for (auto& [pNode, shape] : m_paths) {
		pNode->deleteRelatedNodes();
	}
	m_paths.clear();
}
// 数据准备
void RouterMeshless::setSpecialNetInfomation(string netName) {
	transform(netName.begin(), netName.end(), netName.begin(), ::toupper);
	if (netName == "GND") {
		m_NetNameGND = netName;	//GND网表名称
		const ViaInfo& viaInfo = m_viaInfos->at(m_netsInfos->at(netName).viaName);
		for (const auto& layer : viaInfo.m_layers) {
			if (layer > m_layerGND)
				m_layerGND = layer;		//GND层（取最大层为GND层）
		}
	}
}
void RouterMeshless::extractPinPairs(const STN& steinerTree, vector<pair<PinPad*, PinPad*>>& pairs) {
	// 使用BFS遍历斯坦纳树，收集所有连接关系
	queue<STN> q;
	unordered_set<STN> visited;
	q.emplace(steinerTree);
	visited.insert(steinerTree);
	while (!q.empty()) {
		STN currentNode = q.front();
		q.pop();
		// 处理当前节点的所有子节点
		for (auto& child : currentNode->children) {
			if (visited.find(child) == visited.end()) {
				visited.insert(child);
				q.emplace(child);
				// 添加起点终点对
				if (currentNode->pin && child->pin) {
					setOrderedPinPair(currentNode->pin, child->pin, pairs);
				}
			}
		}
		// 处理父节点（如果是双向连接）
		if (auto parent = currentNode->parent.lock()) {
			if (visited.find(parent) == visited.end()) {
				visited.insert(parent);
				q.emplace(parent);
				if (currentNode->pin && parent->pin) {
					setOrderedPinPair(currentNode->pin, parent->pin, pairs);
				}
			}
		}
	}
}
void RouterMeshless::collectSubtree(STN S, STN T, vector<STN>& S_sub, vector<STN>& T_sub) {
	S_sub.clear();
	T_sub.clear();
	if (!S || !T) return;
	auto T_parent = T->parent.lock();
	auto S_parent = S->parent.lock();
	if (T_parent == S) {
		S->collectSubtreeUp(S_sub, T);
		T->collectSubtreeDown(T_sub);
	}
	else if (S_parent == T) {
		T->collectSubtreeUp(T_sub, S);
		S->collectSubtreeDown(S_sub);
	}
	else {
		std::cerr << "Error: S and T are not direct parent-child\n";
	}
}

void RouterMeshless::setOrderedPinPair(PinPad* pin1, PinPad* pin2, vector<pair<PinPad*, PinPad*>>& pairs) {
	double deltaX = pin1->pos.x - pin2->pos.x;
	if (abs(deltaX) < MapMinValue) {		//竖直线
		double deltaY = pin1->pos.y - pin2->pos.y;
		if (deltaY > MapMinValue) {		//竖直线,下面为起点
			swap(pin1, pin2);
		}
	}
	else if (deltaX > MapMinValue) {		//非竖直线，>取左边的为起点,<取右边的为起点
		swap(pin1, pin2);
	}
	pairs.emplace_back(pin1, pin2);
}
bool RouterMeshless::getPlanningPos(PathNode* node, Point& pos, bool isLine) {
	if (node->direction.vecLength() < 3) {
		double minWireSpacing = m_curNetInfo->width / 2 + node->width / 2;
		double maxClearance = m_curNetInfo->clearance;
		if (node->shape) {	//焊盘顶点
			maxClearance = max(maxClearance, node->shape->clearance);
		}
		minWireSpacing += maxClearance;
		if (isLine) {
			bool prevSameLayer = node->prev ? node->prev->layer == node->layer : false;
			bool nextSameLayer = node->next ? node->layer == node->next->layer : false;
			if (prevSameLayer && nextSameLayer) {
				pos = node->pos + node->direction * (minWireSpacing + maxClearance);
				m_planningPts.insert(pos);

			}
			else {	//跨层，取当前点
				pos = node->pos;
			}
		}
		else {
			pos = node->pos + node->direction * minWireSpacing;
			m_planningPts.insert(pos);
		}
		if (pos.x - minWireSpacing < m_bound[0] || pos.x + minWireSpacing > m_bound[2]
			|| pos.y - minWireSpacing < m_bound[1] || pos.y + minWireSpacing > m_bound[3])
			return false;
	}
	else {
		pos = node->pos;
		m_planningPts.insert(pos);
		//cout << "getPlanningPos error,pos: "<<pos<<" ,node->direction.vecLength= " << node->direction.vecLength() << endl;
		return false;
	}
	return true;
}
void RouterMeshless::mergeNode(PathNode* node) {
	if (!node) return;
	//1.移动到头节点
	// 1. 移动到头节点
	PathNode* head = node;
	while (head->prev)head = head->prev;
	// 找到尾节点
	PathNode* tail = node;
	while (tail->next)tail = tail->next;
	node = head->next;
	//2.合并共线节点
	while (node && node->next) {
		if (node == tail)
			break;
		PathNode* prev = node->prev;
		PathNode* next = node->next;
		if (node->layer != prev->layer || node->layer != next->layer) {		// layer不同直接跳过
			node = next;
			continue;
		}
		if (node->pos.inSameLine(prev->pos, next->pos)) {		// 共线
			node = node->deleteCurruntNode();
		}
		else {
			node = next;
		}
	}
}
void RouterMeshless::findAllPaths(vector<pair<PinPad*, PinPad*>>& pinPairs) {
	//对单一起点终点对布线
	for (int curPathIndex = 0; curPathIndex < pinPairs.size(); curPathIndex++, m_curPathIndex++) {
		auto& pair = pinPairs[curPathIndex];
		//1.准备当前待寻路的路径数据
		if (prepareOnePathData(pair))
			continue;
		//2.设置初始节点和目标节点
		setStartAndEnd();
		//3.特殊的布线
		bool pairNeedToRoute = needToRoute();
		if (!pairNeedToRoute) continue;
		//4.布线
		m_searchTimes = 0;
		bool found = runPPDT();
		if (found)
			m_pathFoundNum++;
		else {
			if (m_curNetName == m_NetNameGND)  //已经设置地线层，则可直接接地
				m_pathFoundNum++;
			else
				m_netFound[m_curNetName] = false;
		}
		if (m_debugEnd)		// 调试中断布线
			return;
	}
	return;
}
void RouterMeshless::findGNDPaths(vector<pair<PinPad*, PinPad*>>& pinPairs) {
	//特殊处理GND
	for (int curPathIndex = 0; curPathIndex < pinPairs.size(); curPathIndex++, m_curPathIndex++) {
		auto& pair = pinPairs[curPathIndex];
		//1.准备当前待寻路的路径数据
		if (prepareOnePathData(pair))
			continue;
		//2.设置初始节点和目标节点
		setStartAndEnd();
		//3.特殊的布线
		//debugBreak(m_node_start);
		bool pairNeedToRoute = needToRoute();
		if (!pairNeedToRoute) continue;
		//4.布线
		bool found = route_GND();
		if (found)
			m_pathFoundNum++;
	}
}
bool RouterMeshless::prepareOnePathData(pair<PinPad*, PinPad*>& pair) {
	//1.当前布线的net名
	m_startPad = pair.first;
	m_endPad = pair.second;
	m_curNetName = m_startPad->netName;
	m_curNetInfo = &m_netsInfos->at(m_curNetName);
	m_viaRadius = m_viaInfos->at(m_curNetInfo->viaName).m_radius;
	m_pathViaMinLength = m_viaRadius * 3;

	//2.起点与终点的邻接斯坦纳点(用于斯坦纳树重构)
	if (m_pinPairExchange)
		collectSubtree(m_steinerQuery[m_startPad->pos], m_steinerQuery[m_endPad->pos], m_startNeibs, m_endNeibs);

	//3.起点、终点层
	m_startLayers.clear();
	m_endLayers.clear();
	for (const auto& [layer, shape] : m_startPad->shapes)
		m_startLayers.insert(layer);
	for (const auto& [layer, shape] : m_endPad->shapes)
		m_endLayers.insert(layer);
	if (m_startLayers.empty() || m_endLayers.empty()) {
		cout << "Path [ " << m_curPathIndex << " ]\t" << "\tError: no layers to run!" << endl;
		return true;
	}
	//4.路径搜索的叶节点重置，已探索的规划点归零
	m_cdNodesList = priority_queue<PathTree*, vector<PathTree*>, ComparePathTreePtr>();
	m_exploredNodes.clear();
	m_candidateObss = queue<PolyShape*>();
	m_queryObssPassed.clear();
	m_querySTChanged.clear();
	return false;
}
void RouterMeshless::setStartAndEnd() {
	m_standartCost = m_startPad->pos.distanceTo(m_endPad->pos) * m_standardCostFactor;
	//1.确定布线层
	int layer_s = *m_startLayers.begin();
	int layer_e = *m_endLayers.begin();
	for (const int& layer : m_startLayers) {
		if (m_endLayers.contains(layer)) {
			layer_s = layer;
			layer_e = layer;
			break;
		}
	}
	//2.在选定的布线层，创建路径树节点
	m_node_start = new PathTree(m_startPad->pos, layer_s, m_curNetName, nullptr);
	m_PinQuaryPad.insert(make_pair(m_node_start, m_startPad));

	m_node_end = new PathTree(m_endPad->pos, layer_e, m_curNetName, nullptr);
	m_PinQuaryPad.insert(make_pair(m_node_end, m_endPad));

	m_pathHeads[m_node_start] = m_curNetName;
	if (m_treesHeads.find(m_curNetName) == m_treesHeads.end())
		m_treesHeads[m_curNetName] = vector<PathTree*>();
	m_treesHeads[m_curNetName].emplace_back(m_node_start);
	m_pathTreesOrdered.emplace_back(m_node_start);

	//3.1起点引出，并设为子节点
	setStartPad(layer_s);
	//4.1终点引出，获取可行的引脚引出点
	setEndPad(layer_e);
}
void RouterMeshless::setStartPad(int layer_s) {
	//3.1起点引出，获取可行的引脚引出点
	PinPad* padPtr;
	int directionType = getPinPadOutDirection(m_node_start, padPtr);
	double minClear = max(m_curNetInfo->clearance, padPtr->shapes[m_node_start->layer].clearance);
	vector<Point> outDirectionsPts;
	bool setStartPadOut = true;
	while (setStartPadOut) {
		setStartPadOut = false;
		if (directionType == 0)			//8个方向引出
			setPadOuPts(m_node_start, m_node_end, minClear, padPtr->box, 0, shape_out_direction[0], outDirectionsPts);
		else if (directionType == 1)	//上下
			setPadOuPts(m_node_start, m_node_end, minClear, padPtr->box, 2, shape_out_direction[1], outDirectionsPts);
		else if (directionType == 2)	//左右
			setPadOuPts(m_node_start, m_node_end, minClear, padPtr->box, 0, shape_out_direction[1], outDirectionsPts);
		else if (directionType == 3) 	//上下左右
			setPadOuPts(m_node_start, m_node_end, minClear, padPtr->box, 0, shape_out_direction[2], outDirectionsPts);
		else {
			cout << "Path [ " << m_curPathIndex << " ]:\t" << "no direction for startPad to rout!" << endl;
			m_cdNodesList.emplace(m_node_start);
		}
		if (outDirectionsPts.empty() && m_pinPairExchange) {
			//当前焊盘没有找到可引出的点
			for (const auto& newSteinerNode : m_startNeibs) {
				if (m_querySTChanged.contains(newSteinerNode->position)) continue;
				if (newSteinerNode->position == m_endPad->pos) continue;
				if (changeStartOrEnd(true, newSteinerNode->pin, layer_s)) {
					setStartPadOut = true;
					break;
				}
			}
		}
	}

	//3.2起点引出，将引出点扩展为子节点
	if (!outDirectionsPts.empty()) {
		vector<PathTree*> outNodes;
		//先设置布线层的引出点
		for (auto& pt : outDirectionsPts) {
			m_planningPts.insert(pt);


			PathTree* newNode = addOneChild(m_node_start, pt, layer_s, nullptr, false);
			if (newNode)
				outNodes.emplace_back(newNode);

			/*bool canReach = isReachable(m_node_start->pos, pt, layer_s, m_startPad, m_endPad);
			if (canReach) {
				PathTree* newNode = addOneChild(m_node_start, pt, layer_s, nullptr, false);
				if (newNode)
					outNodes.emplace_back(newNode);
			}*/
		}
		//再设置其他层引出点
		for (const int& layer : m_routingLayers) {
			if (layer == layer_s) continue;
			else if (m_startLayers.contains(layer)) {	//搜索树在焊盘中心换层，实际路径不需要换层
				PathTree* newStartNode_inLayer = new PathTree(m_startPad->pos, layer, m_curNetName, nullptr);
				double estimateH = m_startPad->pos.distanceTo(m_endPad->pos);
				double stepG = 0;
				if (!m_startPad->shapes.contains(layer))
					stepG = m_standartCost;
				if (!m_endPad->shapes.contains(layer))
					estimateH += m_standartCost;
				m_node_start->addChild(newStartNode_inLayer, stepG, estimateH, 0);
				for (auto& pt : outDirectionsPts) {
					m_planningPts.insert(pt);
					//bool canReach = isReachable(newStartNode_inLayer->pos, pt, layer, m_endPad);
					bool canReach = isReachable(newStartNode_inLayer->pos, pt, layer, m_startPad, m_endPad);
					if (canReach)
						addOneChild(newStartNode_inLayer, pt, layer, nullptr, false);
				}
			}
			else {		//搜索树在引出点处换层，实际路径也要换层
				for (PathTree* node : outNodes) {
					addOneChild(node, node->pos, layer, nullptr, false);
				}
			}
		}
	}
}
void RouterMeshless::setEndPad(int layer_e) {
	PinPad* padPtr;
	int directionType = getPinPadOutDirection(m_node_end, padPtr);
	double minClear = max(m_curNetInfo->clearance, padPtr->shapes[m_node_end->layer].clearance);
	m_endExits.clear();
	if (directionType == 0)			//圆，8个方向引出
		setPadOuPts(m_node_end, m_node_start, minClear, padPtr->box, 0, shape_out_direction[0], m_endExits);
	else if (directionType == 1)	//长方形，上下
		setPadOuPts(m_node_end, m_node_start, minClear, padPtr->box, 2, shape_out_direction[1], m_endExits);
	else if (directionType == 2)	//长方形，左右
		setPadOuPts(m_node_end, m_node_start, minClear, padPtr->box, 0, shape_out_direction[1], m_endExits);
	else if (directionType == 3) 	//接近正方向，上下左右
		setPadOuPts(m_node_end, m_node_start, minClear, padPtr->box, 0, shape_out_direction[2], m_endExits);
	else {
		cout << "Path [ " << m_curPathIndex << " ]:\t" << "no direction for endPad to run!" << endl;
	}
}
void RouterMeshless::setPadOuPts(PathTree* nodeToSet, PathTree* nodeToIgnore, double minClear, const vector<double>& box, int startIdx, int step, vector<Point>& outDirectionsPts) {
	double lineWidth = m_curNetInfo->width / 2;
	double width_x = (box[2] - box[0]) / 2 + minClear + m_viaRadius;		//+ m_viaRadius
	double height_y = (box[3] - box[1]) / 2 + minClear + m_viaRadius;		//+ m_viaRadius
	double max_xy = max(width_x, height_y);
	vector<double> outLengths = { width_x ,max_xy, height_y ,max_xy ,width_x ,max_xy, height_y ,max_xy };
	for (int i = startIdx; i < 8; i += step) {
		const Point& d = Direction8[i];
		Point targetPos = nodeToSet->pos + d * outLengths[i];
		PinPad* nodeToSetPad = nullptr;
		if (m_PinQuaryPad.contains(nodeToSet))
			nodeToSetPad = m_PinQuaryPad[nodeToSet];
		PolyShape* firstShape = getFirstShape(nodeToSet->pos, targetPos, nodeToSet->layer, nodeToSetPad);
		bool canReach = false;
		if (firstShape) {
			PinPad* padEnd = getPadPtr(m_PinQuaryPad[nodeToIgnore]->shapeName);
			if (padEnd->shapes.contains(nodeToSet->layer) && firstShape == &padEnd->shapes[nodeToSet->layer]) {
				// 首个障碍物是终点所在焊盘
				canReach = true;
			}
			else if (firstShape->isLine || firstShape->isVia) {
				canReach = tryToPushAndReach(nodeToSet->pos, targetPos, nodeToSet->layer, firstShape);
			}
		}
		else
			canReach = true;
		if (canReach) {
			outDirectionsPts.emplace_back(targetPos);
			m_planningPts.insert(targetPos);
		}
	}
}
int RouterMeshless::getPinPadOutDirection(PathTree* const nodeSE, PinPad*& padptr) {
	//获取焊盘引出线方向，0-八个任意方向，1-竖向上下引出，2-横向左右引出，3-接近正方形上下左右引出
	if (!m_PinQuaryPad.contains(nodeSE)) {
		cout << "\tError! pinPad has no shape in net [" << nodeSE->netName << "]" << endl;
		return -1;
	}
	const string& shapeName = m_PinQuaryPad[nodeSE]->shapeName;
	padptr = getPadPtr(shapeName);
	const PinPad& pad = *padptr;
	if (pad.r > MapMinValue) {		//圆形焊盘引出，任意标准方向都可以
		return 0;
	}
	else {
		double deltaX = pad.box[2] - pad.box[0];
		double deltaY = pad.box[3] - pad.box[1];
		if (deltaY > rectLengthFactor * deltaX) {		//y方向比较长，竖向长条形 
			return 1;
		}
		else if (deltaX > rectLengthFactor * deltaY) {	//x方向比较长，横向长条形
			return 2;
		}
		else {		//接近正方形
			return 3;
		}
	}
	return 0;
}
bool RouterMeshless::changeStartOrEnd(bool isStart, PinPad* pad, int layer) {
	if (!pad) return false;
	//1.修改斯坦纳树拓扑结构
	auto itChange = m_steinerQuery.find(isStart ? m_startPad->pos : m_endPad->pos);
	auto itHold = m_steinerQuery.find(isStart ? m_endPad->pos : m_startPad->pos);
	if (itChange == m_steinerQuery.end() || itHold == m_steinerQuery.end()) {
		cerr << "Error! SteinerNode not found in changeStartOrEnd\n";
		return false;
	}
	STN steinerToChange = itChange->second;
	STN steinerToHold = itHold->second;
	STN newST = m_steinerQuery[pad->pos];
	if (!steinerToHold->changeTopologyPC(steinerToChange, newST)) {
		cerr << "Error! SteinerNode topology change failed in path:" << m_curPathIndex << endl;
		return false;
	}
	m_querySTChanged.insert(itChange->second->position);
	//2.修改起点终点相关信息
	PathTree newNode(pad->pos, layer, pad->netName, nullptr);
	//2.1更新起点或终点的焊盘与坐标信息
	if (isStart) {
		m_node_start->copyIn(newNode);
		m_startPad = pad;
		m_PinQuaryPad[m_node_start] = pad;
	}
	else {
		m_node_end->copyIn(newNode);
		m_endPad = pad;
		m_PinQuaryPad[m_node_end] = pad;
		setEndPad(layer);
	}
	//3.更新邻接斯坦纳节点
	collectSubtree(m_steinerQuery[m_startPad->pos], m_steinerQuery[m_endPad->pos], m_startNeibs, m_endNeibs);
	//更新代价基数
	m_standartCost = m_node_start->pos.distanceTo(m_node_end->pos) * m_standardCostFactor;
	return true;
}

double RouterMeshless::getPathsLength() {
	double totalLength = 0;
	for (auto& [node, shape] : m_paths) {
		if (!node) continue;
		double length = getOnePathLength(node);
		m_pathLengths[node] = length;
		totalLength += length;
	}
	return totalLength;
}
double RouterMeshless::getOnePathLength(PathNode* head) {
	PathNode* cur = head;
	double length = 0;
	while (cur->next) {
		length += cur->pos.distanceTo(cur->next->pos);
		cur = cur->next;
	}
	return length;
}
int RouterMeshless::getSpecialRoutingType() {
	return 0;
}
bool RouterMeshless::needToRoute() {
	//起点与终点重合，且在同层
	double minPadSize = min(m_startPad->box[2] - m_startPad->box[0], m_startPad->box[3] - m_startPad->box[1]);
	double minPadSize2 = min(m_endPad->box[2] - m_endPad->box[0], m_endPad->box[3] - m_endPad->box[1]);
	minPadSize = max(minPadSize, minPadSize2);
	if (m_node_start->pos.distanceTo(m_node_end->pos) < minPadSize) {
		bool inTheSameLayer = false;
		for (const auto& layer : m_startLayers) {
			if (m_endLayers.contains(layer)) {
				inTheSameLayer = true;
				break;
			}
		}
		if (inTheSameLayer) {
			m_pathFoundNum++;
			return false;
		}
		else {
			return true;
		}
	}
	return true;
}

//PPDT布线算法
bool RouterMeshless::runPPDT() {
	//执行规划点定向生成树搜索算法
	bool pathFound = false;
	//debugBreak(m_node_start);
	while (!m_cdNodesList.empty()) {
		//1选择最优节点
		PathTree* minNode = nodeSelection();
		debugBreak(minNode);
		//2探索，获取目标障碍物列表
		obssExploration(minNode);
		//3扩展子节点
		pathFound = nodeExpansion(minNode);
		m_searchTimes++;
		if (m_searchTimes > m_serchTimesLimit) {
			cout << "time out!" << endl;
			break;
		}
		if (pathFound)//找到路径，跳出循环
			break;
	}
	m_treeNodesSum += m_exploredNodes.size();
	//4回溯，生成路径
	if (pathFound) {
		//回溯，后处理（回溯引脚更新）
		if (m_pinPairExchange)		//1.启用：引脚切换
			checkNewStartNode();
		backTrackOnePath(m_node_end);
		return true;
	}
	else {
		cout << "Path[" << m_curPathIndex << "]\t" << "--> \trouting failed : " << m_node_start->pos << m_node_end->pos << endl;
		return false;
	}
	return false;
}
bool RouterMeshless::route_GND() {
	//焊盘已经接地，不需要布线
	bool startGotGnd = m_GNDConnected.find(m_node_start->pos) != m_GNDConnected.end();
	bool endGotGnd = m_GNDConnected.find(m_node_end->pos) != m_GNDConnected.end();
	if (startGotGnd && endGotGnd) {
		return true;
	}
	//焊盘在GND层，不需布线
	bool startInGndLayer = m_startLayers.contains(m_layerGND);
	bool endInGndLayer = m_endLayers.contains(m_layerGND);
	if (startInGndLayer && endInGndLayer) {
		m_GNDConnected.insert(m_node_start->pos);
		m_GNDConnected.insert(m_node_end->pos);
		return true;
	}
	//只有其中一者已经接地，或者都没有接地
	PolyShape* firstObs = nullptr;
	bool canReach = isReachable(m_node_start->pos, m_node_end->pos, m_node_start->layer, m_startPad, m_endPad, &firstObs);
	if (canReach) {
		m_node_start->addChild(m_node_end, 0, 0, 0);
		// 方向约束
		if (m_4_8Tree && m_directionOp != 0) {
			reconstructDirection(m_node_start, m_node_end, 0);
		}
		//生成一条路径m_node_start到m_node_end的路径
		backTrackOnePath(m_node_end);
		if (!m_GNDOutToGNDLayer)
			return true;
		m_node_end->parent->children.erase(m_node_end);
		m_node_end->parent = nullptr;
		bool alreadyGotGnd = startGotGnd || startInGndLayer || endGotGnd || endInGndLayer;
		if (!alreadyGotGnd) {
			PathTree* viaNode = m_cdNodesList.top();
			double minCost = 999999999;
			//1.在起点处，选择最优的打孔点接地
			while (!m_cdNodesList.empty()) {
				PathTree* selectedNode = m_cdNodesList.top();
				m_cdNodesList.pop();
				if (selectedNode->layer == m_layerGND && selectedNode->g < minCost) {
					minCost = selectedNode->g;
					viaNode = selectedNode;
				}
			}
			//2.在终点处，选择最优的打孔点接地
			Point pt = m_endExits.front();
			Point rst = pt;
			bool found = false;
			int obsSum = getCongestionSum(pt, m_viaRadius, m_node_end->layer, m_layerGND);
			for (int i = 1; i < m_endExits.size(); ++i) {
				pt = m_endExits[i];
				double congestionCostFactor = getCongestionValue(pt, m_viaRadius, m_node_end->layer, m_layerGND);
				if (congestionCostFactor < minCost) {
					bool canReach = isReachable(m_node_end->pos, pt, m_node_end->layer, m_startPad, m_endPad);
					if (canReach) {
						found = true;
						minCost = congestionCostFactor;
						rst = pt;
					}
				}
			}
			if (found) {
				PathTree* newNode = addOneChild(m_node_end, rst, m_node_end->layer, nullptr, false);
				viaNode = addOneChild(newNode, rst, m_layerGND, nullptr, false);
			}
			backTrackOnePath(viaNode);
		}
		m_GNDConnected.insert(m_node_start->pos);
		m_GNDConnected.insert(m_node_end->pos);
	}
	else {		// 无法连接，直接接地
		if (!m_GNDOutToGNDLayer)
			return true;
		if (m_cdNodesList.empty() || m_endExits.empty()) {
			cout << "Path[" << m_curPathIndex << "]\t" << "--> \trouting failed : " << m_node_start->pos << m_node_end->pos << endl;
			return false;
		}
		if (!startGotGnd && !startInGndLayer) {
			//起点没有接地
			PathTree* viaNode = m_cdNodesList.top();
			double minCost = 999999999;
			while (!m_cdNodesList.empty()) {
				PathTree* selectedNode = m_cdNodesList.top();
				m_cdNodesList.pop();
				if (selectedNode->layer == m_layerGND && selectedNode->g < minCost) {
					minCost = selectedNode->g;
					viaNode = selectedNode;
				}
			}
			backTrackOnePath(viaNode);
			m_GNDConnected.insert(m_node_start->pos);
		}
		if (!endGotGnd && !endInGndLayer) {
			//终点没有接地
			Point pt = m_endExits.front();
			Point rst = pt;
			double minCost = getCongestionValue(pt, m_viaRadius, m_node_end->layer, m_layerGND);
			for (int i = 1; i < m_endExits.size(); ++i) {
				pt = m_endExits[i];
				double congestionCostFactor = getCongestionValue(pt, m_viaRadius, m_node_end->layer, m_layerGND);
				if (congestionCostFactor < minCost) {
					bool canReach = isReachable(m_node_end->pos, pt, m_node_end->layer, m_startPad, m_endPad);
					if (canReach) {
						minCost = congestionCostFactor;
						rst = pt;
					}
				}
			}
			PathTree* newNode = addOneChild(m_node_end, rst, m_node_end->layer, nullptr, false);
			PathTree* viaInEndNode = addOneChild(newNode, rst, m_layerGND, nullptr, false);
			backTrackOnePath(viaInEndNode);
			m_GNDConnected.insert(m_node_end->pos);
		}
		//保持树的完整性
		m_node_start->addChild(m_node_end, 0, 0, 0);  //确保一路径对应一棵树
	}
	return true;
}
bool RouterMeshless::isTreeGNDConnected(PathTree* node) {
	if (!node) return false;

	// 1. 当前节点本身在 GND 层
	if (node->layer == m_layerGND)
		return true;

	// 2. 当前节点位置已经被标记接地
	if (m_GNDConnected.find(node->pos) != m_GNDConnected.end())
		return true;

	// 3. 向上找（同一棵树）
	PathTree* pg = node->parent;
	while (pg) {
		if (pg->layer == m_layerGND)
			return true;
		if (m_GNDConnected.find(pg->pos) != m_GNDConnected.end())
			return true;
		pg = pg->parent;
	}
	return false;
}
PathTree* RouterMeshless::nodeSelection() {
	//1.从叶节点中选择最优节点进行扩展
	PathTree* selectedNode = m_cdNodesList.top();
	m_cdNodesList.pop();
	//2.从终点引出点中选择最优目标节点
	m_ptToEnd = m_node_end->pos;
	if (m_endExits.empty()) {
		m_ptToEndCost = 0;
		return selectedNode;
	}
	const Point& pt_s = selectedNode->pos;
	const int& layer = selectedNode->layer;
	int layer1 = layer;
	int layer2 = m_node_end->layer;
	if (layer1 > layer2)
		swap(layer1, layer2);
	//2.1引出点被选择的代价从小到大排序
	int tryTimes = 0;
	multimap<double, Point> ptsToSelect;
	while (tryTimes++ < 2) {
		for (const auto& outPt : m_endExits) {
			bool canReach = isReachable(m_node_end->pos, outPt, layer, m_startPad, m_endPad);
			if (canReach) {
				if (!m_endLayers.contains(layer))
					canReach = isReachable(m_node_end->pos, outPt, m_node_end->layer, m_startPad, m_endPad);
			}
			if (!canReach) continue;
			//计算曼哈顿距离
			Point sTopt = outPt - pt_s;
			double manhattanDistance = fabs(sTopt.x) + fabs(sTopt.y);
			//计算平滑度
			Point vectOut = (m_node_end->pos - outPt).normalizeVec();
			Point vectEnd1 = (outPt - pt_s).normalizeVec();
			double smoothness = vectOut * vectEnd1;
			if (selectedNode->parent) {
				Point vectEnd2 = (pt_s - selectedNode->parent->pos).normalizeVec();
				smoothness += vectEnd1 * vectEnd2;
			}
			smoothness /= 2;
			//综合选择代价
			double cost = manhattanDistance - smoothness * manhattanDistance;
			ptsToSelect.insert(make_pair(cost, outPt));
		}
		//2.2选择代价最小(且可达)的点作为目标点
		if (!ptsToSelect.empty()) {
			m_ptToEndCost = ptsToSelect.begin()->first;
			m_ptToEnd = ptsToSelect.begin()->second;
			break;
		}
		else {
			for (const auto& outPt : m_endExits) {
				bool canVia = checkViaPos(outPt, layer1, layer2);
				if (canVia) break;
			}
			continue;
		}
	}
	return selectedNode;
}
void RouterMeshless::obssExploration(const PathTree* start) {
	//1.获取穿过哪些cells
	const Line line(start->pos, m_ptToEnd);
	vector<GridCell*> cells;
	m_gridManager->getCellsAlongLine2(line, m_curNetInfo->width, cells);
	//2.获取cells中的同层多边形障碍物（可能穿过），去重，去除端点所在的多边形障碍物
	unordered_set<PolyShape*> obss;
	getValidObssInCells(cells, line, start->layer, obss);
	//3.获取相交的障碍物（焊盘，走线）
	getCandidateObss(obss, line);
}
void RouterMeshless::getCandidateObss(const unordered_set<PolyShape*>& obss, const Line& line) {
	//获取相交的障碍物（焊盘，走线）
	for (PolyShape* shape : obss) {
		if (shape->edges.empty()) continue;
		if (shape->isLine) {
			m_candidateObss.emplace(shape);
		}
		else {
			PathNode* head = shape->edges.front().p1;
			PathNode* cur = head;
			if (cur == nullptr) return;
			while (true) {
				PathNode* next = cur->next;  // 环形链表，next 一定存在
				Point pt1 = cur->pos;
				Point pt2 = next->pos;
				Line shapeEdge(pt1, pt2);
				double dist = line.distanceToLine(shapeEdge);
				double minDistPermitted = m_curNetInfo->width / 2 + shape->clearance;
				if (dist < minDistPermitted - MapMinValue) {
					m_candidateObss.emplace(shape);
					break;
				}
				cur = next;
				if (cur == head) break;  // 回到起点，遍历完成
			}
		}
	}
}
void RouterMeshless::getValidObssInCells(const vector<GridCell*>& cells, const Line& line, int layer, unordered_set<PolyShape*>& obss) {
	for (auto cell : cells) {
		//1候选线形障碍物,去重（走线）
		for (auto& [pathline, shape] : cell->getPathLines()) {
			if (pathline->layer != layer) continue;		//忽略不同层的线
			if (pathline->p1->netName == m_curNetName) continue;
			Line shapeEdge(pathline->p1->pos, pathline->p2->pos);
			double dist = line.distanceToLine(shapeEdge);
			double minDistPermitted = m_curNetInfo->width / 2 + pathline->width / 2 + max(m_curNetInfo->clearance, shape->clearance);
			if (dist < minDistPermitted - MapMinValue) {
				obss.insert(shape);
				break;
			}
		}
		//2候选多边形障碍物（焊盘）
		for (PinPad* pad : cell->getPinPads()) {
			if (!pad->shapes.contains(layer)) continue;		//忽略不同层的shape
			if (pad->netName == m_curNetName) continue;	//忽略此net上的过孔
			//if (pad == m_startPad || pad == m_endPad) continue;
			PolyShape* obsShape = &pad->shapes[layer];
			if (obss.find(obsShape) == obss.end()) {
				if (abs(line.Pt1.x - line.Pt2.x) < MapMinValue) {
					//竖直线，用外包盒过滤，去掉左右两侧的多边形
					if (pad->box[0] > line.Pt1.x || pad->box[2] < line.Pt1.x)
						continue;
				}
				else if (abs(line.Pt1.y - line.Pt2.y) < MapMinValue) {
					//水平线，用外包盒过滤，去掉上下两侧的多边形
					if (pad->box[1] > line.Pt1.y || pad->box[3] < line.Pt1.y)
						continue;
				}
				obss.insert(obsShape);
			}
		}
	}
}
bool RouterMeshless::nodeExpansion(PathTree* start) {
	//debugBreak(start);
	if (checkConnectToEnd(start))
		return true;
	while (!m_candidateObss.empty()) {
		PolyShape* obsShape = m_candidateObss.front();
		m_candidateObss.pop();
		m_queryObssPassed.clear();
		nodeExpansionWithObsShape(start, obsShape);
	}
	return false;
}
void RouterMeshless::nodeExpansionWithObsShape(PathTree* start, PolyShape* obsShape) {
	//1.获取阻挡类型（1自阻挡，2中间阻挡，3目标阻挡）
	int blockType = getBlockTypeToEndNode(obsShape, start);
	//2.扩展为子节点
	switch (blockType) {
	case 1:		// 将邻居节点的规划点作为子节点
		setNeibNodeAsChild(start, obsShape); break;
	case 2:		// 将所有规划点作为子节点
		setAllPtsAsChildren(start, obsShape); break;
	case 3:		// 跳过
		break;
	default:
		break;
	}
}
bool RouterMeshless::checkConnectToEnd(PathTree* start) {
	if (m_pinPairExchange) {
		//尝试更换终点
		map<double, PinPad*> newPads;
		checkNewST(start, false, newPads);
		if (!newPads.empty()) {
			PinPad* newPad = newPads.begin()->second;
			if (newPad != m_endPad) {
				changeStartOrEnd(false, newPad, start->layer);
				double stepG = start->pos.distanceTo(m_endPad->pos);
				start->addChild(m_node_end, stepG, 0, 0);
				// 方向约束
				if (m_4_8Tree && m_directionOp != 0) {
					reconstructDirection(start, m_node_end, 0);
				}
				return true;
			}
		}
	}

	if (m_candidateObss.empty()) {		// 可以直达终点
		double stepG = start->pos.distanceTo(m_ptToEnd);
		PathTree* newNode = addOneChild(start, m_ptToEnd, start->layer, nullptr, false);
		// 方向约束
		if (m_4_8Tree && m_directionOp != 0) {
			reconstructDirection(start, newNode, 0);
		}
		if (!newNode) newNode = start;  //目标引脚的引出点与start恰好重合
		if (newNode->layer == m_node_end->layer)				// 同层,直连
			newNode->addChild(m_node_end, m_ptToEndCost, 0, 0);
		else if (m_endLayers.contains(start->layer)) {			// 不同层，但允许原地换层(终点包含该层)
			PathTree* newEndNode_inLayer = new PathTree(m_node_end->pos, newNode->layer, m_curNetName, nullptr);
			newNode->addChild(newEndNode_inLayer, m_ptToEndCost, 0, 0);
			newEndNode_inLayer->addChild(m_node_end, 0, 0, 0);
		}
		else {			//不同层，不允许原地换层（即终点焊盘不含该层）
			PathTree* newEndNode_inLayer = new PathTree(m_ptToEnd, m_node_end->layer, m_curNetName, nullptr);
			newNode->addChild(newEndNode_inLayer, stepG, 0, 0);	//换层节点
			newEndNode_inLayer->addChild(m_node_end, 0, 0, 0);
		}
		return true;
	}

	return false;
}
int RouterMeshless::getBlockTypeToEndNode(PolyShape* shape, PathTree* start)const {
	//获取阻挡类型（1自阻挡，2中间阻挡，3目标阻挡）
	if (start->pNode) {		// 起点是规划点，可能返回1，2，3
		if (start->pNode->shape == shape)
			return 1;
		else if (m_node_end->pNode && m_node_end->pNode->shape == shape)
			return 3;
		else
			return 2;
	}
	else {		// 起点不是规划点（起点是焊盘或者过孔），可能返回2，3
		if (m_node_end->pNode && m_node_end->pNode->shape == shape)
			return 3;
		else
			return 2;
	}
}
void RouterMeshless::setNeibNodeAsChild(PathTree* start, PolyShape* shape) {
	if (!start->pNode) return;
	if (start->pNode->next) {	// 处理next邻居
		Point pos;
		if (getPlanningPos(start->pNode->next, pos, shape->isLine))
			connectToPos(start, pos, start->pNode->next, false);
	}
	if (start->pNode->prev) {	// 处理prev邻居
		Point pos;
		if (getPlanningPos(start->pNode->prev, pos, shape->isLine))
			connectToPos(start, pos, start->pNode->prev, false);
	}
}
void RouterMeshless::setAllPtsAsChildren(PathTree* start, PolyShape* shape) {
	//将目标障碍物的所有规划点添加为子节点（包括多边形和线形障碍物）
	for (const PathLine& pline : shape->edges) {
		Point pos;
		if (getPlanningPos(pline.p1, pos, shape->isLine))
			connectToPos(start, pos, pline.p1, true);
	}
	if (shape->isLine) {
		PathNode* entPt = shape->edges.back().p2;
		Point pos;
		if (getPlanningPos(entPt, pos, true)) {
			connectToPos(start, pos, entPt, true);
		}
	}
}
bool RouterMeshless::connectToPos(PathTree* start, const Point& pos, PathNode* vertexNode, bool inserVia) {
	int layer = start->layer;
	// 1. 更优性判断
	double G = 0, H = 0, E = 0;
	cmpStepCost(start, pos, layer, G, H, E);
	PathTree* oldNode = posGetNode(pos, layer);
	if (oldNode && G >= oldNode->g)
		return false;
	// 2. 几何可达性
	PolyShape* firstObs = nullptr;
	if (!isReachable(start->pos, pos, layer, m_startPad, m_endPad, &firstObs)) {
		if (firstObs && !m_queryObssPassed.contains(firstObs)) {
			m_queryObssPassed.insert(firstObs);
			nodeExpansionWithObsShape(start, firstObs);
		}
		return false;
	}
	// 3. 状态推进（唯一入口）
	PathTree* newNode = addOneChild(start, pos, layer, vertexNode, inserVia);

	// 4. 方向约束
	if (m_4_8Tree && m_directionOp != 0) {
		reconstructDirection(start, newNode, 0);
	}

	return true;
}
PathTree* RouterMeshless::posGetNode(const Point& pos, int layer) {
	auto posIter = m_exploredNodes.find(pos);
	if (posIter != m_exploredNodes.end()) {
		const auto& layerMap = posIter->second;
		auto layerIter = layerMap.find(layer);
		if (layerIter != layerMap.end()) {
			return layerIter->second;
		}
	}
	return nullptr;
}
bool RouterMeshless::cmpStepCost(PathTree* start, const Point& pos, int layer, double& G, double& H, double& E) {
	// ========= 距离函数选择 =========
	auto distance0 = [](const Point& p1, const Point& p2) {
		double dx = p2.x - p1.x;
		double dy = p2.y - p1.y;
		return sqrt(dx * dx + dy * dy);
		};
	auto distance4 = [](const Point& p1, const Point& p2) {
		return abs(p2.x - p1.x) +
			abs(p2.y - p1.y);
		};
	auto distance8 = [](const Point& p1, const Point& p2) {
		double dx = abs(p2.x - p1.x);
		double dy = abs(p2.y - p1.y);
		double a = min(dx, dy);
		double b = max(dx, dy);
		return sqrt(2.0) * a + (b - a);
		};
	auto computeDistance = [&](const Point& p1, const Point& p2) {
		switch (m_directionOp) {
		case 0: return distance0(p1, p2);   // 任意方向
		case 1: return distance4(p1, p2);   // 四方向
		case 2: return distance8(p1, p2);   // 八方向
		default: return distance0(p1, p2);
		}
		};

	double dist = computeDistance(start->pos, pos);
	// ========= 计算 G =========
	if (start->layer == layer)
	{
		if (start->pos == pos)
			G = start->g;
		else
			G = start->g + dist;
	}
	else
	{
		// 过孔
		if (start->pos == pos)
			G = start->g + m_standartCost;
		else
		{
			G = start->g + dist + m_standartCost;
			cout << "via has different xy!" << start->pos << pos << endl;
		}
	}

	// ========= 拥塞代价 =========
	E = start->e + getCongestionValue(pos, m_viaRadius, start->layer, layer);

	// ========= 启发式 H =========
	double hDist = computeDistance(pos, m_node_end->pos);

	if (m_endLayers.contains(layer))
		H = hDist;
	else
		H = hDist + m_standartCost;

	return (start->layer != layer);  // 是否过孔
}

void RouterMeshless::updateExistingNode(PathTree* start, PathTree* oldNode, const double& G, const double& H, const double& E) {
	start->updateChild(oldNode);

	//更新优先队列
	//暂时不实现
}
PathTree* RouterMeshless::addOneChild(PathTree* start, const Point& pos, int layer, PathNode* vertexNode, bool allowVia) {
	double G = 0, H = 0, E = 0;
	cmpStepCost(start, pos, layer, G, H, E);

	PathTree* oldNode = posGetNode(pos, layer);
	if (oldNode) {
		if (G > oldNode->g + MapMinValue)
			return nullptr;
		updateExistingNode(start, oldNode, G, H, E);
		return oldNode;
	}

	// 新节点
	PathTree* newNode = new PathTree(pos, layer, start->netName, vertexNode);
	m_exploredNodes[pos].insert({ layer, newNode });
	start->addChild(newNode, G, H, E);
	m_cdNodesList.emplace(newNode);

	// via 扩展（注意：只对“新节点”）
	if (allowVia && start->pos.distanceTo(pos) > m_pathViaMinLength) {
		for (int otherLayer : m_routingLayers) {
			if (otherLayer == layer) continue;
			double viaG = 0, viaH = 0, viaE = 0;
			if (posGetNode(pos, otherLayer))
				continue;
			cmpStepCost(newNode, pos, otherLayer, viaG, viaH, viaE);
			PathTree* viaNode = new PathTree(pos, otherLayer, start->netName, nullptr);
			m_exploredNodes[pos].insert({ otherLayer, viaNode });
			newNode->addChild(viaNode, viaG, viaH, viaE);
			m_cdNodesList.emplace(viaNode);
		}
	}
	return newNode;
}
double RouterMeshless::computeSmoothness(const Point& prev, const Point& cur, const Point& next) {
	Point v1 = cur - prev;
	Point v2 = next - cur;

	if (v1.vecLength() < MapMinValue ||
		v2.vecLength() < MapMinValue)
		return -1e9;

	v1 = v1.normalizeVec();
	v2 = v2.normalizeVec();

	return v1 * v2;     // dot product，越大越平滑
}
bool RouterMeshless::isStandardDirection(const Point& p1, const Point& p2)const {
	double dx = abs(p2.x - p1.x);
	double dy = abs(p2.y - p1.y);
	if (m_directionOp == 1) {   // 4方向：只允许水平/竖直
		return (dx < MapMinValue || dy < MapMinValue);
	}
	else if (m_directionOp == 2) { // 8方向：允许水平/竖直/45°
		if (dx < MapMinValue || dy < MapMinValue)
			return true;
		if (abs(dx - dy) < MapMinValue)
			return true;
		return false;
	}
	return false;
}

//4/8方向搜素树
void RouterMeshless::reconstructDirection(PathTree* u, PathTree* v, int depth) {
	if (!u || !v || depth > 16) return;
	// 已经是标准方向
	if (isStandardDirection(u->pos, v->pos))
		return;
	// 1️.生成两个候选中间点
	Line line(u->pos, v->pos);
	Point mid1, mid2;
	bool midPosGot = false;
	if (m_directionOp == 1)
		midPosGot = lineMidPoss_4d(line, mid1, mid2);
	else if (m_directionOp == 2)
		midPosGot = lineMidPoss_8d(line, mid1, mid2);
	if (midPosGot) {
		// 1.1计算平滑度（树版本）
		double smooth1 = 0;
		double smooth2 = 0;
		if (u->parent) {
			Point pos1 = u->parent->pos;
			if (pos1 == u->pos && u->parent->parent)
				pos1 = u->parent->parent->pos;
			smooth1 = computeSmoothness(pos1, u->pos, mid1) + computeSmoothness(u->pos, mid1, v->pos);
			smooth2 = computeSmoothness(pos1, u->pos, mid2) + computeSmoothness(u->pos, mid2, v->pos);
		}
		else {
			// 根节点退化处理
			smooth1 = computeSmoothness(u->pos, mid1, v->pos);
			smooth2 = computeSmoothness(u->pos, mid2, v->pos);
		}
		// 1.2平滑优先排序
		Point first = mid1;
		Point second = mid2;
		if (smooth2 > smooth1)
			swap(first, second);
		// 1.3先尝试更平滑点
		auto tryInsert = [&](const Point& pm) -> bool {
			bool canReach =
				isReachable(u->pos, pm, u->layer, m_startPad, m_endPad) &&
				isReachable(pm, v->pos, v->layer, m_startPad, m_endPad);
			if (!canReach) return false;
			// 断开
			u->children.erase(v);
			v->parent = nullptr;
			// 创建节点
			PathTree* midNode = new PathTree(pm, u->layer, u->netName, nullptr);
			// u -> mid
			double G1 = 0, H1 = 0, E1 = 0;
			cmpStepCost(u, pm, u->layer, G1, H1, E1);
			u->addChild(midNode, G1, H1, E1);
			// mid -> v
			double G2 = 0, H2 = 0, E2 = 0;
			cmpStepCost(midNode, v->pos, v->layer, G2, H2, E2);
			midNode->addChild(v, G2, H2, E2);
			midNode->updateTreefgh(m_standartCost);
			// 记录搜索情况，避免不必要的待扩展节点
			if (!posGetNode(pm, u->layer)) {
				m_cdNodesList.emplace(midNode);
				m_exploredNodes[pm].insert({ u->layer, midNode });

				//via扩展，潜在的换层过孔
				//if (u->pos.distanceTo(pm) > m_pathViaMinLength && v->pos.distanceTo(pm) > m_pathViaMinLength) {
				//	for (int otherLayer : m_routingLayers) {
				//		if (otherLayer == midNode->layer) continue;
				//		double viaG = 0, viaH = 0, viaE = 0;
				//		cmpStepCost(midNode, pm, otherLayer, viaG, viaH, viaE);
				//		if (posGetNode(pm, otherLayer))
				//			continue;
				//		PathTree* viaNode = new PathTree(pm, otherLayer, midNode->netName, nullptr);
				//		m_exploredNodes[pm].insert({ otherLayer, viaNode });
				//		midNode->addChild(viaNode, viaG, viaH, viaE);
				//		m_cdNodesList.emplace(viaNode);
				//	}
				//}
			}
			return true;
			};

		if (tryInsert(first))
			return;
		if (tryInsert(second))
			return;
	}

	// 2️.无法直达，绕障碍
	Point po;
	getPostPtsToPass(u->pos, v->pos, u->layer, v->layer, po);
	if (po.x == 0 && po.y == 0)
		return;
	// 2.1 断开原关系
	u->children.erase(v);
	v->parent = nullptr;
	// 2.2 创建绕行节点
	PathTree* obsNode = new PathTree(po, u->layer, u->netName, nullptr);
	m_exploredNodes[po].insert({ u->layer, obsNode });
	// 2.3 u -> obsNode
	double G1 = 0, H1 = 0, E1 = 0;
	cmpStepCost(u, po, u->layer, G1, H1, E1);
	u->addChild(obsNode, G1, H1, E1);
	// 2.4 obsNode -> v
	double G2 = 0, H2 = 0, E2 = 0;
	cmpStepCost(obsNode, v->pos, v->layer, G2, H2, E2);
	obsNode->addChild(v, G2, H2, E2);
	// 2.5 更新子树
	obsNode->updateTreefgh(m_standartCost);
	// 2.6 递归拆分
	reconstructDirection(u, obsNode, depth + 1);
	reconstructDirection(obsNode, v, depth + 1);
}
double RouterMeshless::getCongestionValue(const Point& pos, double r, int layer1, int layer2 = -1) {
	if (m_rangeSize < 1)
		return 0;
	if (layer1 > layer2)
		swap(layer1, layer2);

	// 1.定义范围大小
	vector<double> expValue;
	double minDistance = r + m_curNetInfo->clearance;

	// 2.网格单元
	double minMaxDistance = minDistance * m_rangeSize;        //最大考虑范围半径
	double viaBound0 = pos.x - minMaxDistance;
	double viaBound1 = pos.y - minMaxDistance;
	double viaBound2 = pos.x + minMaxDistance;
	double viaBound3 = pos.y + minMaxDistance;

	vector<GridCell*> cells;
	m_gridManager->getCellsInBox(vector<double>{viaBound0, viaBound1, viaBound2, viaBound3}, cells);
	vector<int> Ni(m_rangeSize + 1, 0);  // Ni[1]...Ni[n]

	// 去重：每个障碍物只算一次
	unordered_set<PinPad*> processedPads;
	for (auto cell : cells) {
		// 2.1多边形障碍物（焊盘）
		for (PinPad* pad : cell->getPinPads()) {
			if (pad->netName == m_curNetName || pad->isVia) continue;
			if (processedPads.count(pad)) continue;
			processedPads.insert(pad);
			if (pad->box[0] > viaBound2 || pad->box[2] < viaBound0 || pad->box[1] > viaBound3 || pad->box[3] < viaBound1)
				continue;
			PolyShape* obsShape = nullptr;
			for (auto& [layer, shape] : pad->shapes) {
				if (layer >= layer1 && layer <= layer2) {
					obsShape = &shape;
					break;
				}
			}
			if (obsShape) {
				// 寻找距离最近的边
				double nearestEdgeDist = minMaxDistance + 1;
				for (auto& edge : obsShape->edges) {
					double dist = pos.distanceToEdge(edge.p1->pos, edge.p2->pos);
					if (dist < nearestEdgeDist) {
						nearestEdgeDist = dist;
					}
				}
				if (nearestEdgeDist < minMaxDistance) {
					// 计算它属于哪一档：1~m_rangeSize
					int band = (int)(nearestEdgeDist / minDistance) + 1;

					if (band < 1) band = 1;
					if (band > m_rangeSize) band = m_rangeSize;

					Ni[band] += 1;
				}
			}
		}
	}
	double obsValue = 0.0;
	for (int i = 1; i <= m_rangeSize; i++) {
		obsValue += (double)Ni[i] * pow(m_decayFactor, i);
	}
	obsValue *= m_standartCost;
	return obsValue;
}
int RouterMeshless::getCongestionSum(const Point& pos, double r, int layer1, int layer2 = -1) const {
	if (layer1 > layer2)
		swap(layer1, layer2);
	vector<GridCell*> cells;
	double minDistance = r + m_curNetInfo->clearance;
	double viaBound0 = pos.x - minDistance;
	double viaBound1 = pos.y - minDistance;
	double viaBound2 = pos.x + minDistance;
	double viaBound3 = pos.y + minDistance;
	m_gridManager->getCellsInBox(vector<double>{viaBound0, viaBound1, viaBound2, viaBound3}, cells);
	int obsSum = 0;
	for (auto cell : cells) {
		//2.1多边形障碍物（焊盘）
		for (PinPad* pad : cell->getPinPads()) {
			if (pad->netName == m_curNetName) continue;
			if (pad->box[0] > viaBound2 || pad->box[2] < viaBound0 || pad->box[1] > viaBound3 || pad->box[3] < viaBound1)
				continue;
			PolyShape* obsShape = nullptr;
			for (auto& [layer, shape] : pad->shapes) {
				if (layer >= layer1 && layer <= layer2) {
					obsShape = &shape;	//找到打孔所穿层的shape
					break;
				}
			}
			if (obsShape) {
				//寻找距离最近的边
				for (auto& edge : obsShape->edges) {
					double dist = pos.distanceToEdge(edge.p1->pos, edge.p2->pos);
					if (dist < minDistance) {
						obsSum += 1;
					}
				}
			}
		}
	}
	return obsSum;//obsSum > 0 ? obsSum - 1 : obsSum
}
PolyShape* RouterMeshless::getFirstShape2(const Point& p1, const Point& p2, int layer, PinPad* ignorePad2) {
	//1.获取穿过哪些cells
	vector<GridCell*> cells;
	Line line(p1, p2);
	m_gridManager->getCellsAlongLine2(line, m_curNetInfo->width, cells);
	unordered_set<PolyShape*> obss;

	//2.获取cells中的同层障碍物（可能穿过），去重，去除端点所在的障碍物
	const NetInfo& netInfo = m_netsInfos->at(m_curNetName);
	double halfLineWidth = netInfo.width / 2;
	for (auto cell : cells) {
		for (PinPad* pad : cell->getPinPads()) {
			if (!pad->shapes.contains(layer)) continue;		//焊盘不在这一层
			if (pad->netName == m_curNetName) continue;		//忽略此net上的过孔
			if (ignorePad2 && pad == ignorePad2) continue;
			PolyShape* curShape = &pad->shapes[layer];
			if (!obss.contains(curShape)) {
				double distToIgnore = halfLineWidth + max(netInfo.clearance, curShape->clearance);
				if (abs(line.Pt1.x - line.Pt2.x) < MapMinValue) {
					//竖直线，用外包盒过滤，去掉左右两侧的多边形
					if (pad->box[0] > line.Pt1.x + distToIgnore || pad->box[2] < line.Pt1.x - distToIgnore)
						continue;
				}
				else if (abs(line.Pt1.y - line.Pt2.y) < MapMinValue) {
					//水平线，用外包盒过滤，去掉上下两侧的多边形
					if (pad->box[1] > line.Pt1.y + distToIgnore || pad->box[3] < line.Pt1.y - distToIgnore)
						continue;
				}
				obss.insert(curShape);
			}
		}
	}

	//3.获取相交的障碍物（多边形焊盘障碍物）
	double nearistObsDist = line.getLength();
	PolyShape* firstObs = nullptr;
	for (PolyShape* shape : obss) {
		if (shape->edges.empty()) continue;
		for (auto& edge : shape->edges) {
			Point pt1 = edge.p1->pos;
			Point pt2 = edge.p2->pos;
			Line shapeEdge(pt1, pt2);
			double minDistPermitted = halfLineWidth + max(netInfo.clearance, shape->clearance);    //允许的最小距离
			double dist = line.distanceToLine(shapeEdge);        // 探索线到障碍物的距离
			if (dist < minDistPermitted - MapMinValue) {    // 被阻挡
				Point crossingPoint = line.getCrossingPoint(shapeEdge);
				double startPtToObsDist = line.Pt1.distanceTo(crossingPoint);
				if (startPtToObsDist < nearistObsDist) {
					nearistObsDist = startPtToObsDist;
					firstObs = shape;
				}
			}
		}
	}

	//4.获取相交的障碍物（线形障碍物）
	for (auto cell : cells) {
		for (auto& [pathline, shape] : cell->getPathLines()) {
			if (pathline->layer != layer) continue;        //忽略不同层的线
			// 同一个net下的线形障碍物
			if (pathline->p1->netName == m_curNetName) continue;

			Line shapeEdge(pathline->p1->pos, pathline->p2->pos);
			double dist = line.distanceToLine(shapeEdge);
			double minDistPermitted = netInfo.width / 2 + pathline->width / 2 + max(netInfo.clearance, shape->clearance);
			if (dist < minDistPermitted - MapMinValue) {
				Point crossingPoint = line.getCrossingPoint(shapeEdge);
				double startPtToObsDist = line.Pt1.distanceTo(crossingPoint);
				double startPtToObsDist2 = line.Pt1.distanceToEdge(pathline->p1->pos, pathline->p2->pos);
				if (startPtToObsDist < nearistObsDist) {
					nearistObsDist = startPtToObsDist;
					firstObs = shape;
				}
			}
		}
	}

	return firstObs;
}
bool RouterMeshless::isReachable2(const Point& p1, const Point& p2, int layer, PinPad* ignorePad2, PolyShape** firstObsPtr) {
	// 获取起点到终点的第一个障碍物
	ignorePad2 = nullptr;
	PolyShape* obs = getFirstShape(p1, p2, layer, ignorePad2);
	if (firstObsPtr)
		*firstObsPtr = obs;
	// 如果没有障碍物，或者障碍物是ignorePad2（终点焊盘），则可达
	return (!obs || (ignorePad2 && ignorePad2->shapes.contains(layer) && obs == &ignorePad2->shapes[layer]));
}
PolyShape* RouterMeshless::getFirstShape(const Point& p1, const Point& p2, int layer, PinPad* ignorePad, PinPad* ignorePad2) {
	//1.获取穿过哪些cells
	vector<GridCell*> cells;
	Line line(p1, p2);
	m_gridManager->getCellsAlongLine2(line, m_curNetInfo->width, cells);
	unordered_set<PolyShape*> obss;

	//2.获取cells中的同层障碍物（可能穿过），去重，去除端点所在的障碍物
	double halfLineWidth = m_curNetInfo->width / 2;
	for (auto cell : cells) {
		for (PinPad* pad : cell->getPinPads()) {
			if (!pad->shapes.contains(layer)) continue;		//焊盘不在这一层
			if (pad->netName == m_curNetName) continue;		//忽略此net上的过孔
			if (ignorePad && pad == ignorePad) continue;	//忽略此焊盘
			if (ignorePad2 && pad == ignorePad2) continue;
			PolyShape* curShape = &pad->shapes[layer];
			if (!obss.contains(curShape)) {
				double distToIgnore = halfLineWidth + max(m_curNetInfo->clearance, curShape->clearance);
				if (abs(line.Pt1.x - line.Pt2.x) < MapMinValue) {
					//竖直线，用外包盒过滤，去掉左右两侧的多边形
					if (pad->box[0] > line.Pt1.x + distToIgnore || pad->box[2] < line.Pt1.x - distToIgnore)
						continue;
				}
				else if (abs(line.Pt1.y - line.Pt2.y) < MapMinValue) {
					//水平线，用外包盒过滤，去掉上下两侧的多边形
					if (pad->box[1] > line.Pt1.y + distToIgnore || pad->box[3] < line.Pt1.y - distToIgnore)
						continue;
				}
				obss.insert(curShape);
			}
		}
	}

	//3.获取相交的障碍物（多边形焊盘障碍物）
	double nearistObsDist = line.getLength() + halfLineWidth * 2 + m_curNetInfo->clearance;
	PolyShape* firstObs = nullptr;
	for (PolyShape* shape : obss) {
		if (shape->edges.empty()) continue;
		double minDistPermitted = halfLineWidth + max(m_curNetInfo->clearance, shape->clearance);    //允许的最小距离 max(m_curNetInfo->clearance, shape->clearance) 或 shape->clearance
		for (auto& edge : shape->edges) {
			Point pt1 = edge.p1->pos;
			Point pt2 = edge.p2->pos;
			Line shapeEdge(pt1, pt2);
			double dist = line.distanceToLine(shapeEdge);        // 探索线到障碍物的距离
			if (dist < minDistPermitted - MapMinValue) {    // 被阻挡
				Point crossingPoint = line.getCrossingPoint(shapeEdge);
				double startPtToObsDist = line.Pt1.distanceTo(crossingPoint);
				if (startPtToObsDist < nearistObsDist) {
					nearistObsDist = startPtToObsDist;
					firstObs = shape;
				}
			}
		}
	}

	//4.获取相交的障碍物（线形障碍物）
	for (auto cell : cells) {
		for (auto& [pathline, shape] : cell->getPathLines()) {
			if (pathline->layer != layer) continue;        //忽略不同层的线
			// 同一个net下的线形障碍物
			if (pathline->p1->netName == m_curNetName) continue;
			Line shapeEdge(pathline->p1->pos, pathline->p2->pos);
			double minDistPermitted = m_curNetInfo->width / 2 + pathline->width / 2 + max(m_curNetInfo->clearance, shape->clearance);
			double dist = line.distanceToLine(shapeEdge);
			if (dist < minDistPermitted - MapMinValue) {
				Point crossingPoint = line.getCrossingPoint(shapeEdge);
				double startPtToObsDist = line.Pt1.distanceTo(crossingPoint);
				double startPtToObsDist2 = line.Pt1.distanceToEdge(pathline->p1->pos, pathline->p2->pos);
				if (startPtToObsDist < nearistObsDist) {
					nearistObsDist = startPtToObsDist;
					firstObs = shape;
				}
			}
		}
	}
	return firstObs;
}
bool RouterMeshless::isReachable(const Point& p1, const Point& p2, int layer, PinPad* ignorePad1, PinPad* ignorePad2, PolyShape** firstObsPtr) {
	// 获取起点到终点的第一个障碍物
	PolyShape* obs = getFirstShape(p1, p2, layer, ignorePad1, ignorePad2);
	if (firstObsPtr)
		*firstObsPtr = obs;
	// 如果没有障碍物，或者障碍物是ignorePad2（终点焊盘），则可达
	return (!obs || (ignorePad2 && ignorePad2->shapes.contains(layer) && obs == &ignorePad2->shapes[layer]));
}
bool RouterMeshless::tryToPushAndReach(const Point& p1, const Point& p2, int layer, PolyShape* obs) {
	// 对p1到p2碰撞检测，找到第一个碰撞的边，如果是焊盘则返回false，如果是线形障碍物则尝试推挤，再次检测，如果成功则返回true
	if (m_pushOn) {
		bool avoidSuccessed = pushToAvoid(p1, p2, m_curNetInfo->width, m_curNetInfo->clearance, obs);
		return avoidSuccessed;
	}
	return false;
}
bool RouterMeshless::pushToAvoid(const Point& p1, const Point& p2, double width, double clear, PolyShape* shape) {
	// 传递的shape是线形障碍物，或者过孔
	Line line(p1, p2);
	if (shape->isLine) {
		//2.推挤形障碍物
		double minDistPermitted = 0.5 * width;
		for (auto& edge : shape->edges) {
			Line shapeEdge(edge.p1->pos, edge.p2->pos);
			minDistPermitted += edge.width * 0.5 + max(clear, shape->clearance);
			double dist = line.distanceToLine(shapeEdge);
			if (dist < minDistPermitted - MapMinValue) {
				Point pushVec = line.getVector().normalizeVec();
				double distToPush = 0;
				Point endgeVect = shapeEdge.getVector().normalizeVec();
				bool parallel = isParallel(pushVec, endgeVect);
				if (parallel) {
					pushVec = Point(pushVec.y, pushVec.x);
					Point linePtToEdgePt = (shapeEdge.Pt1 - line.Pt1).normalizeVec();
					if (pushVec * linePtToEdgePt < 0)
						pushVec = pushVec * (-1);
					distToPush = minDistPermitted - dist;
				}
				else {
					Point crossingPoint = line.getCrossingPoint(shapeEdge);
					double d1 = crossingPoint.distanceTo(line.Pt1);
					double d2 = crossingPoint.distanceTo(line.Pt2);
					double d = minDistPermitted / fabs(cross(pushVec, endgeVect));  // 标准推挤安全距离
					if (d1 < d2) {
						pushVec = pushVec * (-1);
						if (dist > MapMinValue) {
							d1 = -d1;
						}
						distToPush = d + d1;
					}
					else {
						if (dist > MapMinValue) {
							d2 = -d2;
						}
						distToPush = d + d2;
					}
					distToPush = minDistPermitted - dist;
				}
				pushVec = pushVec * distToPush;
				bool pushed = copyAndCheckBeforPush(edge.p1, pushVec, shape);
				if (pushed)
					return true;
			}
		}
		return false;
	}
	else if (shape->isVia) {
		//3.推挤过孔
	}
	return false;
}
bool RouterMeshless::checkPushLine(PathNode* M, PathNode* N, const Point pushVec) {
	Point p1 = M->pos + pushVec;
	Point p2 = N->pos + pushVec;
	int layer = N->layer;
	Line line(p1, p2);
	//1.获取穿过哪些cells
	vector<GridCell*> cells;
	m_gridManager->getCellsAlongLine2(line, m_curNetInfo->width, cells);
	unordered_set<PolyShape*> obss;
	//2.获取cells中的同层障碍物（可能穿过），去重，去除端点所在的障碍物
	const NetInfo& netInfo = m_netsInfos->at(M->netName);
	double halfLineWidth = netInfo.width / 2;
	for (auto cell : cells) {
		for (PinPad* pad : cell->getPinPads()) {
			if (!pad->shapes.contains(layer)) continue;		//焊盘不在这一层
			if (pad->netName == M->netName) continue;		//忽略此net上的过孔
			PolyShape* curShape = &pad->shapes[layer];
			if (!obss.contains(curShape)) {
				double distToIgnore = halfLineWidth + max(netInfo.clearance, curShape->clearance);
				if (abs(line.Pt1.x - line.Pt2.x) < MapMinValue) {
					//竖直线，用外包盒过滤，去掉左右两侧的多边形
					if (pad->box[0] > line.Pt1.x + distToIgnore || pad->box[2] < line.Pt1.x - distToIgnore)
						continue;
				}
				else if (abs(line.Pt1.y - line.Pt2.y) < MapMinValue) {
					//水平线，用外包盒过滤，去掉上下两侧的多边形
					if (pad->box[1] > line.Pt1.y + distToIgnore || pad->box[3] < line.Pt1.y - distToIgnore)
						continue;
				}
				obss.insert(curShape);
			}
		}
	}
	//3.获取相交的障碍物（多边形焊盘障碍物）
	double nearistObsDist = line.getLength();
	for (PolyShape* shape : obss) {
		if (shape->edges.empty()) continue;
		for (auto& edge : shape->edges) {
			Point pt1 = edge.p1->pos;
			Point pt2 = edge.p2->pos;
			Line shapeEdge(pt1, pt2);
			double minDistPermitted = halfLineWidth + max(netInfo.clearance, shape->clearance);    //允许的最小距离
			double dist = line.distanceToLine(shapeEdge);        // 探索线到障碍物的距离
			if (dist < minDistPermitted - MapMinValue) {    // 被阻挡
				return false;
			}
		}
	}
	//4.获取相交的障碍物（线形障碍物）
	for (auto cell : cells) {
		for (auto& [pathline, shape] : cell->getPathLines()) {
			if (pathline->layer != layer) continue;        //忽略不同层的线
			// 同一个net下的线形障碍物
			if (pathline->p1->netName == M->netName) continue;
			Line shapeEdge(pathline->p1->pos, pathline->p2->pos);
			double dist = line.distanceToLine(shapeEdge);
			double minDistPermitted = netInfo.width / 2 + pathline->width / 2 + max(netInfo.clearance, shape->clearance);
			if (dist < minDistPermitted - MapMinValue) {
				return false;
			}
		}
	}
	return true;
}
void RouterMeshless::setSEViasNode(PathNode* head, PathNode* tail) {
	//路径至少三个节点才能确定过孔位置
	if (!head || !head->next || !head->next->next) return;
	if (!tail || !tail->prev || !tail->prev->prev) return;
	//1.起点位置的换层坐标
	if (head->pos == head->next->pos) {
		if (m_startPad->shapes.contains(head->next->layer)) {
			head->layer = head->next->layer;
		}
		else {
			//起点处需要插入过孔
			head->next->pos = head->next->next->pos;
			head->next->layer = head->layer;
		}
	}
	//2.终点位置的换层坐标
	if (tail->pos == tail->prev->pos) {
		if (m_endPad->shapes.contains(tail->prev->layer)) {
			tail->layer = tail->prev->layer;
		}
		else {
			//终点处需要插入过孔
			tail->prev->pos = tail->prev->prev->pos;
			tail->prev->layer = tail->layer;
		}
	}
}
bool RouterMeshless::insertVias(PathNode* head, PathNode* tail) {
	bool insertSuccessed = true;
	PathNode* cur = head;
	while (cur && cur->next) {
		if (cur->pos == cur->next->pos) {
			if (!insertOneVia(cur)) {
				insertSuccessed = false;
				m_DRCCnt++;
			}
		}
		cur = cur->next;
	}
	return insertSuccessed;
}
bool RouterMeshless::insertOneVia(PathNode* viaPre) {
	if (!viaPre || !viaPre->next || viaPre->pos != viaPre->next->pos) {  //
		//cout << "InsertOneVia failed!" << endl;
		return false;
	}
	bool inserted = true;
	if (viaPre->pos == m_node_start->pos) {
		//如果是焊盘处原地换层，实际路径可能不需要换层（焊盘包含该层）
		if (m_startLayers.contains(viaPre->next->layer)) {
			viaPre->layer = viaPre->next->layer;
			return true;
		}
		else { //原地换层不可行，因为焊盘不含该层
			cout << "move start via failed:" << viaPre->pos << endl;
			inserted = false;
			return false;
		}
	}
	else if (viaPre->pos == m_node_end->pos) {
		//如果是焊盘处原地换层，实际路径可能不需要换层（焊盘包含该层）
		if (m_endLayers.contains(viaPre->layer)) {
			viaPre->next->layer = viaPre->layer;
			return true;
		}
		else {
			cout << "move end via failed:" << viaPre->pos << endl;
			inserted = false;
			return false;
		}
	}
	int layer1 = viaPre->layer;
	int layer2 = viaPre->next->layer;
	if (layer1 > layer2)
		swap(layer1, layer2);
	bool canVia = checkViaPos(viaPre->pos, layer1, layer2);		//检查过孔位置，并推移其他干涉的线
	if (!canVia) {
		if (m_pushOn)
			canVia = pushViaAndLine(viaPre, layer1, layer2);		//推移过孔及其所在的线
		else
			return false;
	}
	Point pos = viaPre->pos;
	m_vias.insert(make_pair(pos, PinPad(pos, m_curNetInfo->viaName, m_curNetName, true)));  //true代表过孔
	if (m_curNetName != m_NetNameGND && m_curNetName != m_netNameVCC_1)
		m_viasSum++;
	else if (m_GNDViaCountON)
		m_viasSum++;
	PinPad& onePad = m_vias[pos];
	for (int i = layer1; i <= layer2; i++) {
		onePad.addShape(i, m_viaRadius, { 0,0 });
	}
	m_gridManager->addPinPad(&onePad);
	return inserted;
}
bool RouterMeshless::checkViaPos(const Point& pos, int layer1, int layer2) {
	bool canVia = true;
	vector<GridCell*> cells;
	double minDistance = m_viaRadius + m_curNetInfo->clearance;
	double viaBound0 = pos.x - minDistance;
	double viaBound1 = pos.y - minDistance;
	double viaBound2 = pos.x + minDistance;
	double viaBound3 = pos.y + minDistance;
	m_gridManager->getCellsInBox(vector<double>{viaBound0, viaBound1, viaBound2, viaBound3}, cells);
	for (auto cell : cells) {
		//2.2候选线形障碍物（走线）
		int pushTimies = 0;
		bool tryPushLine = true;
		while (tryPushLine) {
			canVia = true;
			if (pushTimies++ > 5)
				break;
			tryPushLine = false;
			vector<pair<PathLine*, PolyShape*>> pathLinesSnapshot;
			pathLinesSnapshot.reserve(cell->getPathLines().size());
			for (auto& kv : cell->getPathLines()) {
				pathLinesSnapshot.emplace_back(kv);
			}
			for (auto& [pathline, shape] : pathLinesSnapshot) {
				if (!pathline || !shape) continue;
				if (pathline->layer < layer1 || pathline->layer > layer2) continue;     //忽略不同层的线
				if (pathline->p1->netName == m_curNetName) continue;                    // 同一个net下的线形障碍物
				double dist = pos.distanceToEdge(pathline->p1->pos, pathline->p2->pos);
				double minDistPermitted = m_viaRadius + pathline->width / 2 + max(m_curNetInfo->clearance, shape->clearance);
				if (dist < minDistPermitted - MapMinValue) {
					if (m_pushOn) {
						canVia = false;
						PathNode* M = pathline->p1, * N = pathline->p2;
						while (N && N->pos == M->pos) N = N->next;
						if (!N || M->pos == N->pos) continue;
						Point pushVec = (M->pos - N->pos).rotate90().normalizeVec() * (minDistPermitted - dist);
						double distance1 = pos.distanceToEdge(M->pos + pushVec, N->pos + pushVec);
						double distance2 = pos.distanceToEdge(M->pos - pushVec, N->pos - pushVec);
						if (distance1 < distance2)
							pushVec = pushVec * (-1);

						//bool pushed = pushAndUpdate(M, N, pushVec, shape);
						bool pushed = copyAndCheckBeforPush(M, pushVec, shape);

						if (pushed) {
							m_gridManager->getCellsInBox(vector<double>{viaBound0, viaBound1, viaBound2, viaBound3}, cells);
							tryPushLine = true;
						}
						else {
							pushVec = pushVec * (-1);
							//pushed = pushAndUpdate(M, N, pushVec, shape);
							pushed = copyAndCheckBeforPush(M, pushVec, shape);
							if (pushed) {
								m_gridManager->getCellsInBox(vector<double>{viaBound0, viaBound1, viaBound2, viaBound3}, cells);
								tryPushLine = true;
							}
						}
						if (tryPushLine)
							break;
					}
					else {
						return false;
					}
				}
			}
		}
		//2.1多边形障碍物（焊盘）
		for (PinPad* pad : cell->getPinPads()) {
			if (pad->box[0] > viaBound2 || pad->box[2] < viaBound0 || pad->box[1] > viaBound3 || pad->box[3] < viaBound1)
				continue;
			if (pad->netName == m_curNetName)
				continue;
			PolyShape* obsShape = nullptr;
			for (auto& [layer, shape] : pad->shapes) {
				if (layer >= layer1 && layer <= layer2) {
					obsShape = &shape;	//找到打孔所穿层的shape
					break;
				}
			}
			if (obsShape) {
				//寻找距离最近的边
				for (auto& edge : obsShape->edges) {
					double dist = pos.distanceToEdge(edge.p1->pos, edge.p2->pos);
					if (dist < minDistance - MapMinValue) {
						return false;
					}
				}
			}
		}
	}
	return canVia;
}
bool RouterMeshless::pushViaAndLine(PathNode* nodePre, int layer1, int layer2) {
	if (!nodePre->next) return false;
	bool isStart = !nodePre->prev;
	bool isEnd = !nodePre->next->next;
	if (isStart && isEnd) return false;
	if (isStart && !isEnd) {
		return moveSEVia(nodePre, layer1, layer2, isStart);;
	}
	if (isEnd && !isStart) {
		return moveSEVia(nodePre, layer1, layer2, isStart);;
	}
	double moveLength = m_viaRadius - 0.5 * m_curNetInfo->width;
	vector<Point> offsets = {
		Point(moveLength,0),
		Point(0,moveLength),
		Point(-moveLength,0),
		Point(0,-moveLength),
		Point(moveLength,moveLength),
		Point(-moveLength,moveLength),
		Point(-moveLength,-moveLength),
		Point(moveLength,-moveLength) };
	for (const Point& offset : offsets) {
		Point pVia = nodePre->pos + offset;
		//取后面线段或者前面的线段推挤
		Point p2 = nodePre->next->next->pos + offset;
		double projLength2 = fabs(offset * (pVia - p2).normalizeVec().rotate90());
		Point p0 = nodePre->prev->pos + offset;
		double projLength0 = fabs(offset * (pVia - p0).normalizeVec().rotate90());
		if (projLength2 < MapMinValue && projLength0 < MapMinValue) {
			//前后共线，直接移动过孔位置
			bool viaCanReach = checkViaPos(pVia, layer1, layer2);
			//bool pViaInLine = pVia.ptInMidOfEdge(p0, p2);
			if (viaCanReach) {  // && pViaInLine
				nodePre->pos = pVia;
				nodePre->next->pos = pVia;
				return true;
			}
		}
		else {
			//不共线，选择合适的线推挤
			bool viaCanReach = checkViaPos(pVia, layer1, layer2);
			if (viaCanReach) {
				if (projLength2 > projLength0) {
					//bool lineCanReach = isReachable(pVia, p2, viaPre->next->layer, m_endPad);
					bool lineCanReach = isReachable(pVia, p2, nodePre->next->layer, m_startPad, m_endPad);

					if (lineCanReach) {
						pushMoveLine(nodePre, offset);
						return true;
					}
					else {
						//lineCanReach = isReachable(pVia, p0, viaPre->prev->layer, m_endPad);
						lineCanReach = isReachable(pVia, p0, nodePre->prev->layer, m_startPad, m_endPad);
						if (lineCanReach) {
							pushMoveLine(nodePre->prev, offset);
							return true;
						}
					}
				}
				else {
					//bool lineCanReach = isReachable(pVia, p0, viaPre->prev->layer, m_endPad);
					bool lineCanReach = isReachable(pVia, p0, nodePre->prev->layer, m_startPad, m_endPad);
					if (lineCanReach) {
						pushMoveLine(nodePre->prev, offset);
						return true;
					}
					else {
						//lineCanReach = isReachable(pVia, p2, viaPre->prev->layer, m_endPad);
						lineCanReach = isReachable(pVia, p2, nodePre->prev->layer, m_startPad, m_endPad);
						if (lineCanReach) {
							pushMoveLine(nodePre, offset);
							return true;
						}
					}
				}
			}
		}
	}
	m_DRCCnt++;
	cout << "push via failed:" << nodePre->pos << endl;
	return false;
}
bool RouterMeshless::moveSEVia(PathNode* viaPre, int layer1, int layer2, bool isStart) {
	if (!viaPre || !viaPre->next)
		return false;
	Point dir;
	if (isStart) {
		if (!viaPre->next->next)
			return false;
		dir = (viaPre->pos - viaPre->next->next->pos).normalizeVec();
	}
	else {
		if (!viaPre->prev)
			return false;
		dir = (viaPre->pos - viaPre->prev->pos).normalizeVec();
	}
	Point offset = dir * m_viaRadius;
	Point pVia = viaPre->pos + offset;

	bool viaCanReach = checkViaPos(pVia, layer1, layer2);
	if (viaCanReach) {
		viaPre->pos = pVia;
		viaPre->next->pos = pVia;
		return true;
	}
	return false;
}

bool RouterMeshless::backTrackOnePath(PathTree* nodeEnd) {
	PathTree* cur = nodeEnd;
	//2.回溯，生成路径双向链表
	double lineWidth = m_curNetInfo->width;
	PolyShape* nullshape = nullptr;
	PathNode* tail = new PathNode(cur->pos, nullshape, nodeEnd->netName, cur->layer);
	PathNode* n1 = tail;
	n1->width = lineWidth;
	while (cur->parent) {
		if (cur->parent->layer != cur->layer && cur->parent->pos != cur->pos) {
			//不在同一层，坐标也不同，则添加一个过孔
			PathNode* nVia = new PathNode(cur->pos, nullshape, cur->netName, cur->parent->layer);
			nVia->width = lineWidth;
			n1->insertBefore(nVia);
			n1 = nVia;
		}
		PathNode* n2 = new PathNode(cur->parent->pos, nullshape, nodeEnd->netName, cur->parent->layer);
		n2->width = lineWidth;
		n1->insertBefore(n2);
		cur = cur->parent;
		n1 = n2;
	}
	PathNode* head = n1;
	//3.合并共线线段
	mergeNode(head);
	//4.后处理2
	if (m_postOn) {
		if (m_fixWireSpacingOn)
			fixWireSpacing(head);
		if (m_cutAcuteAngle)					//2.启用：尖角裁剪
			cutAngle(head);
	}
	if (!m_4_8Tree && m_directionOp != 0)
		directionStandarlize(head);
	mergeNode(head);
	//5.插入过孔
	bool pathGot = insertVias(head, tail);
	mergeNode(head);
	//6.生成路径
	generateOnePath(head);
	return pathGot;
}
//后处理
void RouterMeshless::checkNewST(PathTree* curStart, bool changeStart, map<double, PinPad*>& newPads) {
	if (!curStart)
		return;
	//1️.确定当前要检查的是 pin_start 还是 pin_target
	PathTree* pinToChange = nullptr;
	PathTree* pinToHold = nullptr;
	vector<STN>* candidates = nullptr;
	double cost = 0.0;
	if (changeStart) {
		pinToChange = m_node_start;
		pinToHold = m_node_end;
		candidates = &m_startNeibs;
		cost = curStart->g;
	}
	else {
		pinToChange = m_node_end;
		pinToHold = m_node_start;
		candidates = &m_endNeibs;
		cost = curStart->h;
	}
	if (!pinToChange || !pinToHold)
		return;
	auto itChange = m_steinerQuery.find(pinToChange->pos);
	auto itHold = m_steinerQuery.find(pinToHold->pos);
	if (itChange == m_steinerQuery.end() || itHold == m_steinerQuery.end())
		return;
	STN steinerToChange = itChange->second;
	STN steinerToHold = itHold->second;

	//3.遍历候选 newST
	for (auto& newST : *candidates) {
		PinPad* newPad = newST->pin;
		if (!newPad)
			continue;
		if (!newPad->shapes.contains(curStart->layer))
			continue;
		double stepG = curStart->pos.distanceTo(newPad->pos);
		if (stepG >= cost)
			continue;

		//bool canReach = isReachable(curStart->pos, newPad->pos, curStart->layer, newPad);
		bool canReach = isReachable(curStart->pos, newPad->pos, curStart->layer, m_startPad, newPad);
		if (!canReach)
			continue;
		newPads.insert(make_pair(stepG, newPad));
	}
	return;
}
void RouterMeshless::checkNewStartNode() {		//2.回溯过程，引脚对换
	if (!m_node_end || !m_node_start) return;
	PathTree* cur = m_node_end;
	while (cur->parent) {
		map<double, PinPad*> newPads;
		checkNewST(cur, true, newPads);
		if (!newPads.empty()) {
			PinPad* newPad = newPads.begin()->second;
			if (newPad == m_startPad)
				return;
			cur->parent->children.erase(cur);
			cur->parent = nullptr;
			unordered_set<PathTree*>& children = m_node_start->children;
			while (!m_node_start->children.empty()) {
				auto it = m_node_start->children.begin();
				PathTree* child = *it;
				child->remove();
			}
			m_node_start->children.clear();
			//更新树根节点
			changeStartOrEnd(true, newPad, cur->layer);
			double stepG = cur->pos.distanceTo(newPad->pos);
			m_node_start->addChild(cur, stepG, -1, 0);	//此处设置一个任意的estimateH=-1，下一行更新
			double viaCost = m_standartCost;
			m_node_start->updateTreefgh(viaCost);
			// 方向约束
			if (m_4_8Tree && m_directionOp != 0) {
				reconstructDirection(m_node_start, cur, 0);
			}
			return;
		}
		cur = cur->parent;
	}
}
void RouterMeshless::fixWireSpacing(PathNode* head) {
	if (!head || !head->next || !head->next->next || !head->next->next->next) return;
	double minSelfDist = m_curNetInfo->clearance + m_curNetInfo->width;
	PathNode* start = nullptr, * end = nullptr;
	PathNode* n1 = head, * n2 = head->next;
	PathNode* n3 = n2->next, * n4 = n3->next;
	bool needFix = true;
	while (needFix) {
		needFix = false;
		//1.寻找到距离过近且索引最远的两条线段start，end
		for (; n2 && n2->next; n1 = n2, n2 = n2->next) {
			if (n1->layer != n2->layer) continue;
			for (; n4; n3 = n4, n4 = n4->next) {
				if (n3->layer != n4->layer) continue;
				if (n1->layer != n3->layer) continue;
				Line L1(n1->pos, n2->pos);
				Line L2(n3->pos, n4->pos);
				double dist = L1.distanceToLine(L2);
				if (dist < minSelfDist) {
					start = n1;
					end = n4;
				}
			}
			if (n1)
				break;
		}
		//2.设置两条线段的连接关系
		if (start && end) {
			if (start->next->next->next == end) {
				n1 = n2;
				n2 = n2->next;
				continue;	//只间隔一条线段
			}
			else {
				Line L1(start->pos, start->next->pos);
				Line L2(end->prev->pos, end->pos);
				bool parallel = isParallel(L1.getVector().normalizeVec(), L2.getVector().normalizeVec());
				if (parallel) {
					//删除start与end之间的节点
					PathNode* cur = start->next;
					while (cur && cur != end) {
						PathNode* next = cur->next;
						cur->deleteCurruntNode(); // 安全删除当前节点
						cur = next;
					}
				}
				else {
					Point crossPt = L1.getCrossingPoint(L2);
					//将start->next节点坐标改为crossPt，删除后面的节点，一直到end
					if (start->next) {
						start->next->pos = crossPt;
					}
					// 删除 start->next->next 到 end 的节点
					PathNode* cur = start->next ? start->next->next : nullptr;
					while (cur && cur != end) {
						PathNode* next = cur->next;
						cur->deleteCurruntNode();
						cur = next;
					}
				}
				n1 = end;
				n2 = end->next;
				continue;
			}
		}
	}
}
void RouterMeshless::getPostPtsToPass(const Point& p1, const Point& p2, int layer1, int layer2, Point& ptToBypass) {
	Line line(p1, p2);
	double x1 = p1.x, y1 = p1.y;    // 外包盒
	double x2 = p2.x, y2 = p2.y;
	double minX = min(x1, x2) + MapMinValue;    // 缩小有效的规划点区域，防止重复将当前顶点加入
	double maxX = max(x1, x2) - MapMinValue;
	double minY = min(y1, y2) + MapMinValue;
	double maxY = max(y1, y2) - MapMinValue;

	// 1. 获取搜索哪些cells
	vector<GridCell*> cells;
	m_gridManager->getCellsInBox({ minX, minY, maxX, maxY }, cells);

	// 2. 获取cells中的同层的：多边形障碍物、线形障碍物
	unordered_set<PolyShape*> obss;
	for (auto cell : cells) {
		// 2.1 候选多边形，去重，去除端点所在的多边形障碍物（焊盘）
		for (PinPad* pad : cell->getPinPads()) {
			if (!pad->shapes.contains(layer1)) continue;    // 忽略不同层的shape
			PolyShape* obsShape = &pad->shapes[layer1];

			// 忽略端点所在多边形
			bool isStartPad = m_startPad->shapes.contains(layer1) && obsShape == &m_startPad->shapes[layer1];
			bool isEndPad = m_endPad->shapes.contains(layer2) && obsShape == &m_endPad->shapes[layer2];
			if (isStartPad || isEndPad)
				continue;
			if (obss.find(obsShape) == obss.end()) {
				obss.insert(obsShape);
			}
		}

		// 2.2 候选线形障碍物，去重（走线）
		for (auto& [pathline, shape] : cell->getPathLines()) {
			if (pathline->layer != layer1) continue;        // 忽略不同层的线
			// 同一个net下的线形障碍物
			if (pathline->p1->netName == m_curNetName) continue;
			obss.insert(shape);
		}
	}

	// 3. 获取候选障碍物在矩形区域内的规划点（焊盘、线形）
	double minDistance = numeric_limits<double>::max();
	Point nearistPoint;
	for (PolyShape* shape : obss) {
		if (shape->edges.empty()) continue;
		for (auto& edge : shape->edges) {    // 焊盘、线形障碍物，相同的筛选逻辑
			Point pos;
			if (getPlanningPos(edge.p1, pos, shape->isLine)) {
				if (pos.x >= minX && pos.x <= maxX && pos.y >= minY && pos.y <= maxY) {
					double pointToLineDistance = line.distanceToPoint(pos);
					if (pointToLineDistance < minDistance) {
						minDistance = pointToLineDistance;
						nearistPoint = pos;
					}
				}
			}
		}
	}

	// 4. 有序存入obssToPass
	ptToBypass = nearistPoint;
}
bool RouterMeshless::lineMidPoss_4d(const Line& line, Point& midPos1, Point& midPos2) {
	double x1 = line.Pt1.x;
	double y1 = line.Pt1.y;
	double x2 = line.Pt2.x;
	double y2 = line.Pt2.y;
	double dx = x2 - x1;
	double dy = y2 - y1;
	// 如果已经是水平或竖直，不需要处理
	if (abs(dx) < MapMinValue ||
		abs(dy) < MapMinValue)
	{
		return false;
	}
	// 方案1：先水平后竖直
	midPos1.x = x2;
	midPos1.y = y1;
	// 方案2：先竖直后水平
	midPos2.x = x1;
	midPos2.y = y2;
	return true;
}
bool RouterMeshless::lineMidPoss_8d(const Line& line, Point& midPos1, Point& midPos2) {
	//将一条斜线转化成两条标准方向走线
	double x1 = line.Pt1.x, y1 = line.Pt1.y;
	double x2 = line.Pt2.x, y2 = line.Pt2.y;
	double dx = x2 - x1;
	double dy = y2 - y1;
	double abs_dx = abs(dx);
	double abs_dy = abs(dy);
	if (abs_dx < MapMinValue || abs_dy < MapMinValue || abs(abs_dx - abs_dy) < MapMinValue) {	// 水平、竖直、45度走线，不处理
		return false;
	}
	if (abs_dy > abs_dx) {		// dy更大
		int step_y = dy > 0 ? 1 : -1;
		// 处理方式1，靠近起点走水平竖直线
		midPos1.x = x1;
		midPos1.y = y2 - abs_dx * step_y;
		// 处理方式2，靠近终点走水平竖直线
		midPos2.x = x2;
		midPos2.y = y1 + abs_dx * step_y;
	}
	else {		// dx更大
		int step_x = dx > 0 ? 1 : -1;
		// 处理方式1，靠近起点走水平竖直线
		midPos1.y = y1;
		midPos1.x = x2 - abs_dy * step_x;
		// 处理方式2，靠近终点走水平竖直线
		midPos2.y = y2;
		midPos2.x = x1 + abs_dy * step_x;
	}
	return true;
}

void RouterMeshless::cutAngle(PathNode* const head) {   // 5.尖角裁剪（不考虑换层处的尖角）
	if (!head || !head->next) return;
	const double cos45 = sqrt(2) / 2;
	PathNode* cur = head->next;
	while (cur && cur->next) {
		Point Bpos = cur->pos;
		PathNode* A = cur->prev;
		PathNode* C = cur->next;
		if (!A || !C) {
			cur = cur->next;
			continue;
		}
		// 如果邻居节点坐标与B相同，说明不是有效拐点
		if (A->pos == Bpos || C->pos == Bpos) {
			cur = cur->next;
			continue;
		}
		// ---------- 计算两条边 ----------
		Line line1(A->pos, Bpos);
		Line line2(Bpos, C->pos);
		double length1 = line1.getLength();
		double length2 = line2.getLength();
		if (length1 <= MapMinValue || length2 <= MapMinValue) {
			cur = cur->next;
			continue;
		}
		// ---------- 计算角度 ----------
		Point vec1 = line1.getVector() / length1;
		Point vec2 = line2.getVector() / length2;
		double consAng = vec1 * vec2;
		PathNode* prevReal = A;
		PathNode* nextReal = C;
		bool is90Angle = abs(consAng) < MapMinValue;
		if (is90Angle) {
			if (cut90Angle(cur, prevReal, nextReal, vec1, vec2))
				cur = prevReal;
		}
		else {
			bool is135Angle = abs(consAng + cos45) < MapMinValue;
			if (is135Angle) {
				if (cut45Angle(cur, prevReal, nextReal, vec1, vec2))
					cur = prevReal;
			}
		}
		cur = cur->next;
	}
}
void RouterMeshless::cutAngle_ml(PathNode* const head) {   // 5.尖角裁剪（换层的尖角也考虑在内）
	if (!head || !head->next) return;
	const double cos45 = sqrt(2) / 2;
	PathNode* cur = head->next;
	while (cur && cur->next) {
		Point Bpos = cur->pos;
		// ---------- 找A点(前方第一个坐标不同的节点) ----------
		PathNode* A = cur->prev;
		while (A && A->pos == Bpos) A = A->prev;
		if (!A) {
			cur = cur->next;
			continue;
		}
		// ---------- 找C点(后方第一个坐标不同的节点) ----------
		PathNode* C = cur->next;
		while (C && C->pos == Bpos) C = C->next;
		if (!C) {
			cur = cur->next;
			continue;
		}
		// ---------- 计算两条边 ----------
		Line line1(A->pos, Bpos);
		Line line2(Bpos, C->pos);
		double length1 = line1.getLength();
		double length2 = line2.getLength();
		if (length1 <= MapMinValue || length2 <= MapMinValue) {
			cur = cur->next;
			continue;
		}
		// ---------- 计算角度 ----------
		Point vec1 = line1.getVector() / length1;
		Point vec2 = line2.getVector() / length2;
		double consAng = vec1 * vec2;
		PathNode* prevReal = A;
		PathNode* nextReal = C;
		bool is90Angle = abs(consAng) < MapMinValue;
		if (is90Angle) {
			if (cut90Angle(cur, prevReal, nextReal, vec1, vec2))
				cur = prevReal;
		}
		else {
			bool is135Angle = abs(consAng + cos45) < MapMinValue;
			if (is135Angle) {
				if (cut45Angle(cur, prevReal, nextReal, vec1, vec2))
					cur = prevReal;
			}
		}
		cur = cur->next;
	}
}
bool RouterMeshless::cut90Angle(PathNode* cur, PathNode* prevReal, PathNode* nextReal, const Point& vec1, const Point& vec2)const {
	if (!prevReal || !nextReal) return false;
	double maxWidth = max(prevReal->width, nextReal->width);
	double miterLen = maxWidth * m_miter;
	Point pM = cur->pos - vec1 * miterLen;
	Point pN = cur->pos + vec2 * miterLen;
	cur->pos = pM;
	PathNode* nodeN = new PathNode(pN, nullptr, cur->netName, cur->layer);
	cur->insertAfter(nodeN);
	PathNode* t = nodeN->next;
	while (t && (t->pos == cur->pos)) {
		t->pos = pN;
		t = t->next;
	}
	return true;
}
bool RouterMeshless::cut45Angle(PathNode* cur, PathNode* prevReal, PathNode* nextReal, const Point& vec1, const Point& vec2)const {
	if (!prevReal || !nextReal) return false;
	PathNode* A = prevReal;
	PathNode* B = cur;
	PathNode* C = nextReal;
	double lenAB = Line(A->pos, B->pos).getLength();
	double lenBC = Line(B->pos, C->pos).getLength();
	if (lenAB < MapMinValue || lenBC < MapMinValue)
		return false;
	double maxWidth = max(A->width, C->width);
	double miterLen = maxWidth * m_miter;

	if (miterLen < MapMinValue)
		return false;
	double Lcut = miterLen * (1 + sqrt(2));
	double minRemain = miterLen;
	double shortLen = min(lenAB, lenBC);
	double longLen = max(lenAB, lenBC);
	bool AB_short = lenAB < lenBC;
	Point pM, pN;
	PathNode* start = A;
	PathNode* end = C;
	// CASE 1 对称切角
	if (lenAB >= Lcut + minRemain && lenBC >= Lcut + minRemain) {
		pM = B->pos - vec1 * Lcut;
		pN = B->pos + vec2 * Lcut;
		Point pP = pM + vec1 * miterLen * sqrt(2) + vec2 * miterLen;
		PathNode* nodeP = new PathNode(pP, nullptr, B->netName, B->layer);
		nodeP->width = B->width;
		PathNode* nodeN = new PathNode(pN, nullptr, B->netName, B->layer);
		nodeN->width = B->width;
		// 更新 AB
		B->pos = pM;
		// 插入 P
		B->insertAfter(nodeP);
		// 插入 N
		nodeP->insertAfter(nodeN);
		// 更新 BC
		PathNode* t = nodeN->next;
		while (t && (t->pos == B->pos))
		{
			t->pos = pN;
			t = t->next;
		}

		return true;
	}
	// CASE 2 / 3 非对称
	double shortCut = shortLen;
	double longCut;
	double diag = shortLen / (sqrt(2) / 2);
	if (longLen >= diag)
		longCut = diag;
	else
		longCut = shortLen * (sqrt(2) / 2);
	if (AB_short) {
		pM = A->pos;
		pN = B->pos + vec2 * longCut;
	}
	else {
		pM = B->pos - vec1 * longCut;
		pN = C->pos;
	}
	//--------------------------------
	// 更新节点
	//--------------------------------
	if (AB_short) {
		B->pos = pM;
		PathNode* t = B->next;

		while (t && (t->pos == B->pos))
		{
			t->pos = pN;
			t = t->next;
		}
	}
	else {
		B->pos = pM;
	}
	return true;
}
void RouterMeshless::generateOnePath(PathNode* const head) {
	//1.生成路径线段集合
	vector<PathLine> pathLines;
	double lineWidth = m_curNetInfo->width;
	PathNode* cur = head;
	while (cur->next) {
		cur->width = lineWidth;
		if (cur->layer != cur->next->layer && cur->pos != cur->next->pos) {
			cout << "layer error when generate the path:" << cur->pos << cur->next->pos << endl;
		}
		pathLines.emplace_back(cur, cur->next, cur->layer, lineWidth);  //m_curNetName
		cur = cur->next;
	}
	//2.建立头节点-->路径shape映射，尾节点-->路径shape映射
	m_paths.insert(make_pair(head, PolyShape(pathLines, 0, true, m_curNetInfo->clearance, m_curNetName)));	// true线形障碍物
	PolyShape* pathShape = &m_paths[head];
	cur = head;
	while (cur->next) {
		cur->shape = pathShape;
		cur = cur->next;
	}
	//6.设置路径的规划点方向
	pathShape->setDirection();
	//7.将路径添加到网格中
	m_gridManager->addShapeLines(pathShape);

	//================================
	//起点终点，插入一个孔，仅用于第一章实验绘图标记
	//Point poss = m_node_start->pos;
	//m_viasSum++;
	//m_vias.insert(make_pair(poss, PinPad(poss, m_curNetInfo->viaName, m_curNetName)));
	//PinPad& onePads = m_vias[poss];
	//onePads.addShape(1, m_viaRadius, { 0,0 });
	//m_gridManager->addPinPad(&onePads);
	//起点终点，插入一个孔，仅用于第章实验
	//Point pose = cur->pos;
	//m_viasSum++;
	//m_vias.insert(make_pair(pose, PinPad(pose, m_curNetInfo->viaName, m_curNetName)));
	//PinPad& onePade = m_vias[pose];
	//onePade.addShape(1, m_viaRadius, { 0,0 });
	//m_gridManager->addPinPad(&onePade);

	//8.多线程剪枝
	{
		lock_guard<mutex> lock(m_mutex);
		m_idxQueue.push(m_curPathIndex);		//入参需修改为实际路径索引+1
	}
	m_cv.notify_one();
}

//后处理方向约束
void RouterMeshless::directionStandarlize(PathNode* head) {    // 方向标准化（针对PathNode双向链表）
	if (!head || !head->next) return; // 至少需要两个节点
	PathNode* cur = head;
	// 从链表头开始，处理每一对相邻节点
	while (cur) {
		m_postTimes = 0;
		PathNode* nextNode = cur->next;   // 先保存原始后继
		reconstructDirection(cur, nextNode, 0);
		cur = nextNode;
	}
}
void RouterMeshless::reconstructDirection(PathNode* start, PathNode* end, int depth) {
	if (!start || !end || depth > 30)
		return;

	// 已经满足当前方向模式
	if (isStandardDirection(start->pos, end->pos))
		return;

	Line line(start->pos, end->pos);

	// 1. 生成两个候选中间点
	Point mid1, mid2;
	bool midPosGot = false;
	if (m_directionOp == 1)
		midPosGot = lineMidPoss_4d(line, mid1, mid2);
	else if (m_directionOp == 2)
		midPosGot = lineMidPoss_8d(line, mid1, mid2);

	if (midPosGot) {
		// 1.1 计算平滑度
		double smooth1 = 0.0;
		double smooth2 = 0.0;

		if (start->prev) {
			Point prevPos = start->prev->pos;
			smooth1 =
				computeSmoothness(prevPos, start->pos, mid1) +
				computeSmoothness(start->pos, mid1, end->pos);
			smooth2 =
				computeSmoothness(prevPos, start->pos, mid2) +
				computeSmoothness(start->pos, mid2, end->pos);
		}
		else {
			smooth1 = computeSmoothness(start->pos, mid1, end->pos);
			smooth2 = computeSmoothness(start->pos, mid2, end->pos);
		}

		// 1.2 平滑优先排序
		Point first = mid1;
		Point second = mid2;
		if (smooth2 > smooth1)
			std::swap(first, second);

		// 1.3 先尝试更平滑的候选点
		auto tryInsert = [&](const Point& pm) -> bool {
			bool canReach =
				isReachable(start->pos, pm, start->layer, m_startPad, m_endPad) &&
				isReachable(pm, end->pos, end->layer, m_startPad, m_endPad);

			if (!canReach)
				return false;
			PathNode* midNode = new PathNode(pm, nullptr, start->netName, start->layer);
			start->insertAfter(midNode);

			reconstructDirection(start, midNode, depth + 1);
			reconstructDirection(midNode, end, depth + 1);
			return true;
			};
		if (tryInsert(first))
			return;
		if (tryInsert(second))
			return;
	}

	// 2. 无法直达，寻找绕障规划点
	Point po(0, 0);
	getPostPtsToPass(start->pos, end->pos, start->layer, end->layer, po);

	if (po.x == 0 && po.y == 0)
		return;

	// 2.1 插入绕障点
	PathNode* obsNode = new PathNode(po, nullptr, start->netName, start->layer);
	start->insertAfter(obsNode);

	// 2.2 递归拆分
	reconstructDirection(start, obsNode, depth + 1);
	reconstructDirection(obsNode, end, depth + 1);
}


//推挤算法
void RouterMeshless::pushAndUpdate(PathNode* pushNode, const Point& pushVec, PolyShape* shape) {
	m_gridManager->removeOnePath(shape);
	PathNode* start = shape->edges.front().p1;
	PathNode* end = shape->edges.back().p2;
	removeVias(start, end);
	pushMoveLine(pushNode, pushVec, shape);
	if (shape && !shape->edges.empty())
		addOnePathToGrid(shape);
	resetVias(start, end);
}
bool RouterMeshless::checkBeforPush(PathNode* node, PolyShape* shape, PolyShape* shapeTmp, const Point& pushVec, bool setPath) {
	//1.可行性判断
	bool canPush = pushMoveLineTmpToCheck(node, pushVec, shapeTmp);
	if (!canPush)
		return false;

	//2.正式推挤，并更新空间网格索引
	if (setPath) {
		pushAndUpdate(node, pushVec, shape);
	}
	return true;
}
bool RouterMeshless::copyAndCheckBeforPush(PathNode* pushNode, const Point& pushVec, PolyShape* shape) {
	PolyShape* shapeTmp = shape->copy();
	bool pushed = checkBeforPush(pushNode, shape, shapeTmp, pushVec, true);	// true:碰撞检测后，如果临时数据可以推挤，则推挤算法数据
	shapeTmp->deleteNodes();
	delete shapeTmp;
	shapeTmp = nullptr;
	return pushed;
}
void RouterMeshless::pushMoveLine(PathNode* node, const Point& offset, PolyShape* shape) {
	if (!node || !node->next) return;
	// 1. 找到推挤线段的完整范围：M-N
	PathNode* M = node;
	PathNode* N = node->next;
	while (N && N->pos == M->pos) N = N->next;
	if (!M || !N || M == N) return;

	// 2. 计算推挤向量
	Point lineVec = N->pos - M->pos;
	if (lineVec.vecLength() < MapMinValue) return;
	Point normal(-lineVec.y, lineVec.x);
	normal = normal.normalizeVec();
	double projLength = offset * normal;
	if (fabs(projLength) < MapMinValue || fabs(projLength) > 1000)
		return;
	Point projOffset = normal * projLength;

	// 3.1 计算受影响的节点范围与最终坐标(起点)
	Point newM = M->pos + projOffset;	//平移之后的位置M
	Point newN = N->pos + projOffset;	//平移之后的位置N
	Point vecPrePush = (newN - newM).normalizeVec();
	getMPrevNodeToChange(projLength, normal, M);	// M-->M'
	Point tPosM, tPosN;
	if (M->prev) {
		PathNode* M_pre = M->prev;
		while (M_pre && M_pre->prev && M_pre->pos == M->pos) M_pre = M_pre->prev;
		tPosM = lineIntersection(newM, newN, M->pos, M_pre->pos);	//延长相交的位置M
	}
	else {	//M是第一个点
		M->insertAfter(new PathNode(M));
		M = M->next;
		tPosM = outCrosingPos(newM, newN, M->pos, true);
	}
	// 3.1 计算受影响的节点范围与最终坐标(终点)
	getNNextNodeToChange(projLength, normal, N);	// N-->N'
	if (N->next) {
		PathNode* N_next = N->next;
		while (N_next && N_next->next && N_next->pos == N->pos) N_next = N_next->next;
		tPosN = lineIntersection(newM, newN, N->pos, N_next->pos);	//延长相交的位置N
	}
	else {	//N是最后一个点
		N->insertBefore(new PathNode(N));
		N = N->prev;
		tPosN = outCrosingPos(newN, newM, N->pos, true);
	}
	Point vecAfterPush = tPosN - tPosM;
	double lenAfterPush = vecAfterPush.vecLength();
	if (lenAfterPush < MapMinValue)
		vecAfterPush = Point(0, 0);
	else
		vecAfterPush = vecAfterPush / lenAfterPush;
	if (vecPrePush * vecAfterPush < 0) {	// 推挤过度，导致方向反向
		Point crossingPt = lineIntersection(M->pos, tPosM, N->pos, tPosN);
		tPosM = crossingPt;
		tPosN = crossingPt;
	}
	if (isnan(tPosM.x) || isnan(tPosM.y) || isnan(tPosN.x) || isnan(tPosN.y))
		return;
	// 4.设置连接关系
	setNodeConnections(M, N, tPosM, tPosN, shape);
	// 5.检查路径链表
	if (shape)
		checkPathData(shape->edges.front().p1, shape->edges.back().p2);
}
bool RouterMeshless::pushMoveLineTmpToCheck(PathNode* nodeInputLock, const Point& offset, PolyShape* shapeTmp) {
	// 创建一个副本进行推挤，不改变原来数据
	PathNode* node = shapeTmp->edges.front().p1;
	while (node) {
		if (node->pos == nodeInputLock->pos && node->layer == nodeInputLock->layer)
			break;
		node = node->next;
	}
	if (!node || !node->next) return false;
	// 1. 找到推挤线段的完整范围：M-N
	PathNode* M = node;
	PathNode* N = node->next;
	while (N && N->pos == M->pos) N = N->next;
	if (!M || !N || M == N) return false;

	// 2. 计算推挤向量
	Point lineVec = N->pos - M->pos;
	if (lineVec.vecLength() < MapMinValue) return false;
	Point normal(-lineVec.y, lineVec.x);
	normal = normal.normalizeVec();
	double projLength = offset * normal;
	if (fabs(projLength) < MapMinValue || fabs(projLength) > 1000)
		return false;
	Point projOffset = normal * projLength;

	// 3.1 计算受影响的节点范围与最终坐标(起点)
	Point newM = M->pos + projOffset;	//平移之后的位置M
	Point newN = N->pos + projOffset;	//平移之后的位置N
	Point vecPrePush = (newN - newM).normalizeVec();
	getMPrevNodeToChange(projLength, normal, M);	// M-->M'
	Point tPosM, tPosN;
	if (M->prev) {
		PathNode* M_pre = M->prev;
		while (M_pre && M_pre->pos == M->pos) M_pre = M_pre->prev;
		tPosM = lineIntersection(newM, newN, M->pos, M_pre->pos);	//延长相交的位置M
	}
	else {	//M是第一个点
		M->insertAfter(new PathNode(M));
		M = M->next;
		tPosM = outCrosingPos(newM, newN, M->pos, true);
	}
	// 3.1 计算受影响的节点范围与最终坐标(终点)
	getNNextNodeToChange(projLength, normal, N);	// N-->N'
	if (N->next) {
		PathNode* N_next = N->next;
		while (N_next && N_next->pos == N->pos) N_next = N_next->next;
		tPosN = lineIntersection(newM, newN, N->pos, N_next->pos);	//延长相交的位置N
	}
	else {	//N是最后一个点
		N->insertBefore(new PathNode(N));
		N = N->prev;
		tPosN = outCrosingPos(newN, newM, N->pos, true);
	}
	Point vecAfterPush = tPosN - tPosM;
	double lenAfterPush = vecAfterPush.vecLength();
	if (lenAfterPush < MapMinValue)
		vecAfterPush = Point(0, 0);
	else
		vecAfterPush = vecAfterPush / lenAfterPush;
	if (vecPrePush * vecAfterPush < 0) {	// 推挤过度，导致方向反向
		Point crossingPt = lineIntersection(M->pos, tPosM, N->pos, tPosN);
		tPosM = crossingPt;
		tPosN = crossingPt;
	}
	if (isnan(tPosM.x) || isnan(tPosM.y) || isnan(tPosN.x) || isnan(tPosN.y)) {
		return false;
	}
	const Point M_pos = M->pos;
	const Point N_pos = N->pos;
	const int M_layer = M->layer;
	const int N_layer = N->layer;
	PathNode* M_prev = M->prev ? M->prev : M;
	PathNode* N_next = N->next ? N->next : N;
	// 4.设置连接关系
	setNodeConnections(M, N, tPosM, tPosN, shapeTmp);
	// 5.检查路径链表数据是否合法（成环，断裂等）
	checkPathData(shapeTmp->edges.front().p1, shapeTmp->edges.back().p2);
	// 6.碰撞检测并返回
	if (collisionCheck(shapeTmp->edges.front().p1, shapeTmp->edges.back().p2))
		return true;
	return false;
}

void RouterMeshless::pushLineDataUpdate(const PolyShape* shapeCopy, PolyShape* pathShape) {
	vector<PathLine>& algPath = pathShape->edges;
	PathNode* start = algPath.front().p1;
	PathNode* end = algPath.back().p2;
	removeVias(start, end);
	removeOnePathFromGrid(pathShape);
	//更新算法数据
	const vector<PathLine> pathInput = shapeCopy->edges;
	int erase_num = (int)(algPath.size() - pathInput.size());
	if (erase_num > 0) {
		PathNode* end = algPath.back().p2;
		algPath.erase(algPath.end() - erase_num, algPath.end());
		for (int i = 0; i < erase_num; ++i) {
			end->prev->removeFromList();
		}
		algPath.back().p2 = end;
	}
	else if (erase_num < 0) {
		PathNode* end = algPath.back().p2;
		PathNode* end_1 = algPath.back().p1;
		algPath.pop_back();
		size_t push_size = pathInput.size() - algPath.size();
		for (int i = erase_num; i < 0; ++i) {
			PathNode* end_insert = new PathNode({ 0,0 }, nullptr, end_1->netName, 0);
			end_1->insertAfter(end_insert);
			algPath.emplace_back(PathLine(end_1, end_insert, 0, 0));
			end_1 = end_insert;
		}
		algPath.emplace_back(PathLine(end_1, end, 0, 0));
	}
	algPath[0].p1->pos = pathInput[0].p1->pos;
	algPath[0].layer = pathInput[0].p1->layer;
	for (int i = 0; i < algPath.size(); ++i) {
		algPath[i].width = pathInput[i].width;
		algPath[i].layer = pathInput[i].layer;
		algPath[i].p2->pos = pathInput[i].p2->pos;
		algPath[i].p2->layer = pathInput[i].p2->layer;
	}
	pathShape->setDirection();
	addOnePathToGrid(pathShape);
	//// 更新过孔位置
	//if (!m_vias) return;
	//vector<CircleUI>& ui_vias = m_data->m_viaCircles;
	//ui_vias.clear();
	//for (const auto& [pos, pad] : *m_vias) {
	//	CircleUI ccPin(pos.x, pos.y, pad.r);
	//	ui_vias.emplace_back(ccPin);
	//}
	start = algPath.front().p1;
	end = algPath.back().p2;
	resetVias(start, end);
}
Point RouterMeshless::lineIntersection(const Point& p1, const Point& p2, const Point& p3, const Point& p4) {
	double x1 = p1.x, y1 = p1.y;
	double x2 = p2.x, y2 = p2.y;
	double x3 = p3.x, y3 = p3.y;
	double x4 = p4.x, y4 = p4.y;

	double denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
	if (fabs(denom) < MapMinValue) {
		// 平行或重合，返回中间点
		std::cout << "Parallel or coincident lines" << endl;
		return Point((x1 + x3) / 2, (y1 + y3) / 2);
	}

	double px = ((x1 * y2 - y1 * x2) * (x3 - x4) - (x1 - x2) * (x3 * y4 - y3 * x4)) / denom;
	double py = ((x1 * y2 - y1 * x2) * (y3 - y4) - (y1 - y2) * (x3 * y4 - y3 * x4)) / denom;

	return Point(px, py);
}
bool RouterMeshless::isParallel(const Point& v1, const Point& v2)const {
	double v1LenSq = v1.x * v1.x + v1.y * v1.y;
	double v2LenSq = v2.x * v2.x + v2.y * v2.y;
	if (v1LenSq < MapMinValue || v2LenSq < MapMinValue)
		return true;
	double dot = v1.x * v2.x + v1.y * v2.y;
	double cosSq = (dot * dot) / (v1LenSq * v2LenSq);
	return cosSq > 0.998001;
}
bool RouterMeshless::isParallel(const Point& p1, const Point& p2, const Point& p3, const Point& p4)const {
	return isParallel(p1 - p2, p3 - p4);
}
Point RouterMeshless::outCrosingPos(const Point& tPos1, const Point& tPos2, const Point& SE, bool istPos1) {
	vector<Point> directions = {
		Point(1, 0),   // 0度
		Point(1, 1),   // 45度
		Point(0, 1),   // 90度
		Point(-1, 1),  // 135度
	};
	Point vec;
	if (istPos1)
		vec = (tPos1 - tPos2).normalizeVec();
	else
		vec = (tPos2 - tPos1).normalizeVec();
	vector<Point> intersections;
	for (const auto& dir : directions) {
		Point SE_end = SE + dir;
		if (abs(cross(dir, vec)) < MapMinValue)
			continue;	//忽略平行线
		Point intersection = lineIntersection(tPos1, tPos2, SE, SE_end);
		intersections.emplace_back(intersection);
	}
	Point bestIntersection = Point((tPos1.x + tPos2.x) / 2, (tPos1.y + tPos2.y) / 2);
	double maxSmoothness = -MapMinValue;
	for (const Point& intersection : intersections) {
		Point vecNew = (SE - intersection).normalizeVec();
		double smoothness = vec * vecNew;
		if (smoothness > maxSmoothness) {
			maxSmoothness = smoothness;
			bestIntersection = intersection;
		}
	}
	return bestIntersection;
}

void RouterMeshless::removeVias(PathNode* node1, PathNode* node2) {
	if (!node1) return;
	while (node1->next && node1 != node2) {
		if (node1->pos == node1->next->pos) {
			auto it = m_vias.find(node1->pos);
			if (it != m_vias.end()) {
				PinPad& viaToRemove = it->second;
				m_gridManager->removePinPad(&viaToRemove);
				m_vias.erase(it);
			}
		}
		node1 = node1->next;
	}

}
void RouterMeshless::resetVias(PathNode* node1, PathNode* node2) {
	if (!node1) return;
	const string& netName = node1->netName;
	auto it = m_netsInfos->find(netName);
	if (it == m_netsInfos->end()) return;
	const NetInfo& netInfo = it->second;
	while (node1->next && node1 != node2) {
		if (node1->pos == node1->next->pos) {
			m_vias.insert(make_pair(node1->pos, PinPad(node1->pos, "viasX", netName)));
			PinPad& newVia = m_vias[node1->pos];
			int layer1 = node1->layer;
			int layer2 = node1->next->layer;
			if (layer1 > layer2) swap(layer1, layer2);
			for (int layer = layer1; layer <= layer2; layer++) {
				newVia.addShape(layer, m_viaRadius, { 0,0 });
			}
			m_gridManager->addPinPad(&newVia);
		}
		node1 = node1->next;
	}

}
void RouterMeshless::getConnectionPoints(PathNode* M, PathNode* N, PathNode*& D, PathNode*& E) {
	D = nullptr;
	E = nullptr;

	// 向前找D
	PathNode* curr = M;
	while (curr) {
		if (curr->prev && curr->prev->pos != curr->pos) {
			D = curr->prev;
			break;
		}
		curr = curr->prev;
	}

	// 向后找E
	curr = N;
	while (curr) {
		if (curr->next && curr->next->pos != curr->pos) {
			E = curr->next;
			break;
		}
		curr = curr->next;
	}
}
void RouterMeshless::getMPrevNodeToChange(const double& projLength, const Point& normal, PathNode*& M) {
	PathNode* nodeToChange = M->prev;
	PathNode* rst = M;
	while (nodeToChange) {
		Point vecMD = (nodeToChange->pos - M->pos);
		double prjMD = vecMD * normal;
		if (prjMD * projLength < -MapMinValue) {		//推挤位置反向
			break;
		}
		else {
			if (abs(projLength) >= abs(prjMD) + MapMinValue) { 	//推挤位置超过nodeToChange
				rst = nodeToChange;
				nodeToChange = nodeToChange->prev;
			}
			else 	//推挤位置未超过nodeToChange
				break;
		}
	}
	M = rst;
}
void RouterMeshless::getNNextNodeToChange(const double& projLength, const Point& normal, PathNode*& N) {
	PathNode* nodeToChange = N->next;
	PathNode* rst = N;
	while (nodeToChange) {
		Point vecNE = (nodeToChange->pos - N->pos);
		double prjNE = vecNE * normal;
		if (prjNE * projLength < -MapMinValue) {
			break;
		}
		else {
			if (abs(projLength) >= abs(prjNE) + MapMinValue) {
				rst = nodeToChange;
				nodeToChange = nodeToChange->next;
			}
			else
				break;
		}
	}
	N = rst;
}
void RouterMeshless::setNodeConnections(PathNode* Node1, PathNode* Node2, const Point& tPos1, const Point& tPos2, PolyShape* shape) {
	if (!Node1 || !Node2)
		return;
	//端点处的换层节点
	PathNode* cur = Node1;
	Point origPos = Node1->pos;
	while (cur->pos == origPos) {
		cur->pos = tPos1;
		cur = cur->next;
	}
	cur = Node2;
	origPos = Node2->pos;
	while (cur->pos == origPos) {
		cur->pos = tPos2;
		cur = cur->prev;
	}
	cur = Node1;
	while (Node1 && Node1 != Node2->next) {
		//计算待修改节点到slideLine的投影点，并判断投影点位置
		Point shadePos = Node1->pos.shadePointToLine(tPos1, tPos2);
		Point vec1 = tPos1 - shadePos;
		Point vec2 = tPos2 - shadePos;
		//如果投影点在线段内，则移到投影点，如果不在线段内，则移到线段端点
		if (vec1 * vec2 < MapMinValue)
			Node1->pos = shadePos;
		else if (vec1.vecLength() < vec2.vecLength())
			Node1->pos = tPos1;
		else
			Node1->pos = tPos2;
		Node1 = Node1->next;
	}
	//合并共线节点
	while (cur->prev) cur = cur->prev;
	mergeNode(cur);
	//更新shapeEdges
	if (!shape || shape->edges.empty()) return;
	vector<PathLine>* pathLines = &shape->edges;
	if (!pathLines) return;
	double lineWidth = pathLines->at(0).width;
	pathLines->clear();
	while (cur->next) {
		if (cur->layer != cur->next->layer && cur->pos != cur->next->pos) {
			cout << "layer error in func [setNodeConnections]:" << cur->pos << cur->next->pos << endl;
		}
		pathLines->emplace_back(cur, cur->next, cur->layer, lineWidth);  //m_curNetName
		cur = cur->next;
	}
}
bool RouterMeshless::collisionCheck(const PathNode* M, const PathNode* N) {
	//碰撞检测，返回是否可通行，有碰撞返回false（不可通行），无碰撞返回true（可通行）
	const string& netName = M->netName;
	const NetInfo& netInfo = m_netsInfos->at(netName);
	double halfLineWidth = netInfo.width / 2;
	while (M != N) {
		if (M->pos.x - halfLineWidth < m_bound[0] || M->pos.x + halfLineWidth > m_bound[2]
			|| M->pos.y - halfLineWidth < m_bound[1] || M->pos.y + halfLineWidth > m_bound[3])
			return false;
		PathNode* M_next = M->next;
		int layer = M->layer;
		if (layer == M_next->layer) {
			Line line(M->pos, M_next->pos);
			//1.获取穿过哪些cells
			vector<GridCell*> cells;
			m_gridManager->getCellsAlongLine2(line, m_curNetInfo->width, cells);
			unordered_set<PolyShape*> obss;
			//2.获取cells中的同层障碍物（可能穿过），去重，去除端点所在的障碍物
			for (auto cell : cells) {
				for (PinPad* pad : cell->getPinPads()) {
					if (!pad->shapes.contains(layer)) continue;		//焊盘不在这一层
					if (pad->netName == netName) continue;		//忽略此net上的过孔
					PolyShape* curShape = &pad->shapes[layer];
					if (!obss.contains(curShape)) {
						double distToIgnore = halfLineWidth + max(netInfo.clearance, curShape->clearance);
						if (abs(line.Pt1.x - line.Pt2.x) < MapMinValue) {
							//竖直线，用外包盒过滤，去掉左右两侧的多边形
							if (pad->box[0] > line.Pt1.x + distToIgnore || pad->box[2] < line.Pt1.x - distToIgnore)
								continue;
						}
						else if (abs(line.Pt1.y - line.Pt2.y) < MapMinValue) {
							//水平线，用外包盒过滤，去掉上下两侧的多边形
							if (pad->box[1] > line.Pt1.y + distToIgnore || pad->box[3] < line.Pt1.y - distToIgnore)
								continue;
						}
						obss.insert(curShape);
					}
				}
			}
			//3.获取相交的障碍物（多边形焊盘障碍物）
			double nearistObsDist = line.getLength();
			for (PolyShape* shape : obss) {
				if (shape->edges.empty()) continue;
				for (auto& edge : shape->edges) {
					Point pt1 = edge.p1->pos;
					Point pt2 = edge.p2->pos;
					Line shapeEdge(pt1, pt2);
					double minDistPermitted = halfLineWidth + max(netInfo.clearance, shape->clearance);    //允许的最小距离
					double dist = line.distanceToLine(shapeEdge);        // 探索线到障碍物的距离
					if (dist < minDistPermitted - MapMinValue) {    // 被阻挡
						return false;
					}
				}
			}
			//4.获取相交的障碍物（线形障碍物）
			for (auto cell : cells) {
				for (auto& [pathline, shape] : cell->getPathLines()) {
					if (pathline->layer != layer) continue;        //忽略不同层的线
					// 同一个net下的线形障碍物
					if (pathline->p1->netName == netName) continue;
					Line shapeEdge(pathline->p1->pos, pathline->p2->pos);
					double dist = line.distanceToLine(shapeEdge);
					double minDistPermitted = netInfo.width / 2 + pathline->width / 2 + max(netInfo.clearance, shape->clearance);
					if (dist < minDistPermitted - MapMinValue) {
						return false;
					}
				}
			}
		}
		else {
			//过孔，暂时不检测
		}
		M = M_next;
	}
	return true;
}
void RouterMeshless::checkPathData(PathNode* head, PathNode* tail) {
	if (!head || !tail) {
		cerr << "head or tail is null" << endl;
		abort();
	}
	// 检查节点坐标是否合法
	auto isValidCoordinate = [](double val) -> bool {
		return !std::isnan(val) && !std::isinf(val) && std::isfinite(val);
		};
	// head 必须没有 prev
	if (head->prev != nullptr) {
		cerr << "head->prev is not null" << endl;
		abort();
	}
	PathNode* node = head;
	int cnt = 0;
	while (node) {
		// next-prev一致性检查
		if (node->next && node->next->prev != node) {
			cerr << "链表断裂 (next->prev mismatch)" << endl;
			abort();
		}
		// prev-next一致性检查
		if (node->prev && node->prev->next != node) {
			cerr << "链表断裂 (prev->next mismatch)" << endl;
			abort();
		}
		if (!isValidCoordinate(node->pos.x) || !isValidCoordinate(node->pos.y)) {
			cerr << "Node has invalid coordinates: (" << node->pos.x << ", " << node->pos.y << ")" << "\thead:(" << head->pos.x << ", " << head->pos.y << ")" << endl;
			abort();
		}
		node = node->next;

		// 成环检测
		if (++cnt > 10000) {
			cerr << "链表可能成环" << endl;
			abort();
		}
	}
	// 最后一个节点必须是 tail
	PathNode* last = head;
	while (last->next)
		last = last->next;
	if (last != tail) {
		cerr << "tail mismatch: last node is not tail" << endl;
		abort();
	}
	// tail 必须没有 next
	if (tail->next != nullptr) {
		cerr << "tail->next is not null" << endl;
		abort();
	}
}

//多线程剪枝
void RouterMeshless::workerLoop()
{
	while (!m_stop) {
		int idx;
		{
			unique_lock<mutex> lock(m_mutex);
			m_cv.wait(lock, [&]() {
				return !m_idxQueue.empty() || m_stop;
				});

			if (m_stop) return;

			idx = m_idxQueue.front();
			m_idxQueue.pop();
		}
		cut(idx);
	}
}
void RouterMeshless::cut(int idx) {
	// 对 tree 的子节点做剪枝
	// 不改 vector，不改 tree 头节点地址
}

//几何重构
double RouterMeshless::getMergedPathsLength() {
	m_pathLines.clear();

	// 1. 从 m_paths 中按 net 收集所有原始边
	unordered_map<string, vector<Line>> netRawLines = collectNetLinesFromPaths();

	// 2. 对每个 net 单独处理
	for (auto& [netName, rawLines] : netRawLines) {
		if (rawLines.empty()) continue;

		// 2.1 按共线关系分组
		vector<vector<Line>> groups = groupCollinearLines(rawLines);

		// 2.2 对每组原子化，并收集去重后的边
		unordered_set<EdgeKey, EdgeKeyHash> uniqueEdges;
		vector<Line> mergedLines;

		for (const auto& group : groups) {
			vector<Line> atomicLines = atomizeAndMergeCollinearGroup(group);

			for (const auto& seg : atomicLines) {
				if (seg.getPt1() == seg.getPt2()) continue;

				EdgeKey key(seg.getPt1(), seg.getPt2());
				if (uniqueEdges.find(key) == uniqueEdges.end()) {
					uniqueEdges.insert(key);
					mergedLines.emplace_back(key.a, key.b, 0, seg.width);
				}
			}
		}

		m_pathLines[netName] = std::move(mergedLines);
	}

	// 3. 统计总长度
	double totalLength = 0.0;
	for (const auto& [netName, lines] : m_pathLines) {
		for (const auto& line : lines) {
			totalLength += line.getPt1().distanceTo(line.getPt2());
		}
	}

	return totalLength;
}
unordered_map<string, vector<Line>> RouterMeshless::collectNetLinesFromPaths() const {
	unordered_map<string, vector<Line>> netRawLines;

	for (const auto& [head, shape] : m_paths) {
		if (!head) continue;

		// 按你的要求，用路径节点的 netName 作为 key
		string netName = shape.shapeName;
		if (netName.empty()) continue;

		PathNode* cur = head;
		while (cur && cur->next) {
			const Point& p1 = cur->pos;
			const Point& p2 = cur->next->pos;

			if (!(p1 == p2)) {
				// 这里不考虑层，但 Line 需要 layer/width，就给个默认值
				netRawLines[netName].emplace_back(p1, p2, 0, cur->width);
			}
			cur = cur->next;
		}
	}

	return netRawLines;
}
bool RouterMeshless::areSameInfiniteLine(const Line& l1, const Line& l2) const {
	const Point& a = l1.getPt1();
	const Point& b = l1.getPt2();
	const Point& c = l2.getPt1();
	const Point& d = l2.getPt2();

	// 零长度边直接不参与
	if (a == b || c == d) return false;

	// 先判断平行（含反向平行）
	if (!isParallel(a, b, c, d)) return false;

	// 再判断是否共线：c 和 d 都应在 ab 所在直线上
	// 用 Point::inSameLine，自带容差判断
	return a.inSameLine(b, c) && a.inSameLine(b, d);
}
vector<vector<Line>> RouterMeshless::groupCollinearLines(const vector<Line>& lines) const {
	vector<vector<Line>> groups;

	for (const auto& line : lines) {
		if (line.getPt1() == line.getPt2()) continue;

		bool placed = false;
		for (auto& group : groups) {
			if (group.empty()) continue;

			if (areSameInfiniteLine(group.front(), line)) {
				group.push_back(line);
				placed = true;
				break;
			}
		}

		if (!placed) {
			groups.push_back({ line });
		}
	}

	return groups;
}
double RouterMeshless::pointParamOnLine(const Point& origin, const Point& dirUnit, const Point& p) const {
	return (p - origin) * dirUnit;
}
bool RouterMeshless::segmentCoveredByAnyLine(const Point& a, const Point& b, const vector<Line>& linesOnSameLine) const {
	if (a == b) return false;
	if (linesOnSameLine.empty()) return false;

	const Line& ref = linesOnSameLine.front();
	Point origin = ref.getPt1();
	Point dir = ref.getPt2() - ref.getPt1();
	Point dirUnit = dir.normalizeVec();

	if (dirUnit.vecLength() < MapMinValue) return false;

	double ta = pointParamOnLine(origin, dirUnit, a);
	double tb = pointParamOnLine(origin, dirUnit, b);
	double segL = std::min(ta, tb);
	double segR = std::max(ta, tb);

	for (const auto& line : linesOnSameLine) {
		double t1 = pointParamOnLine(origin, dirUnit, line.getPt1());
		double t2 = pointParamOnLine(origin, dirUnit, line.getPt2());
		double lineL = std::min(t1, t2);
		double lineR = std::max(t1, t2);

		// 候选原子段 [segL, segR] 完全落在某原始段 [lineL, lineR] 中
		if (segL >= lineL - MapMinValue && segR <= lineR + MapMinValue) {
			return true;
		}
	}

	return false;
}
vector<Line> RouterMeshless::atomizeAndMergeCollinearGroup(const vector<Line>& group) const {
	vector<Line> result;
	if (group.empty()) return result;

	// 找一条非零长度边作参考
	int refIdx = -1;
	for (int i = 0; i < (int)group.size(); ++i) {
		if (!(group[i].getPt1() == group[i].getPt2())) {
			refIdx = i;
			break;
		}
	}
	if (refIdx == -1) return result;

	const Line& ref = group[refIdx];
	Point origin = ref.getPt1();
	Point dir = ref.getPt2() - ref.getPt1();
	Point dirUnit = dir.normalizeVec();

	if (dirUnit.vecLength() < MapMinValue) return result;

	// 1. 收集所有端点，并转成参数
	vector<pair<double, Point>> paramPts;
	paramPts.reserve(group.size() * 2);

	for (const auto& line : group) {
		const Point& p1 = line.getPt1();
		const Point& p2 = line.getPt2();
		if (p1 == p2) continue;

		double t1 = pointParamOnLine(origin, dirUnit, p1);
		double t2 = pointParamOnLine(origin, dirUnit, p2);
		paramPts.emplace_back(t1, p1);
		paramPts.emplace_back(t2, p2);
	}

	if (paramPts.size() < 2) return result;

	// 2. 按参数排序
	std::sort(paramPts.begin(), paramPts.end(),
		[](const pair<double, Point>& lhs, const pair<double, Point>& rhs) {
			return lhs.first < rhs.first;
		});

	// 3. 参数去重（保留几何点）
	vector<pair<double, Point>> uniqueParamPts;
	uniqueParamPts.reserve(paramPts.size());

	for (const auto& tp : paramPts) {
		if (uniqueParamPts.empty()) {
			uniqueParamPts.push_back(tp);
		}
		else {
			if (std::fabs(tp.first - uniqueParamPts.back().first) > MapMinValue) {
				uniqueParamPts.push_back(tp);
			}
			else {
				// 参数相同则认为是同一点，不重复加入
				// 这里保留先加入的那个点即可
			}
		}
	}

	if (uniqueParamPts.size() < 2) return result;

	// 4. 相邻参数点构成候选原子段，检查是否被任意原始线段覆盖
	unordered_set<EdgeKey, EdgeKeyHash> uniqueEdges;

	for (size_t i = 0; i + 1 < uniqueParamPts.size(); ++i) {
		const Point& p1 = uniqueParamPts[i].second;
		const Point& p2 = uniqueParamPts[i + 1].second;

		if (p1 == p2) continue;

		if (segmentCoveredByAnyLine(p1, p2, group)) {
			EdgeKey key(p1, p2);
			if (uniqueEdges.find(key) == uniqueEdges.end()) {
				uniqueEdges.insert(key);
				result.emplace_back(key.a, key.b, 0, 0);
			}
		}
	}

	return result;
}