#include "MST.h"
#include "../src_basics/RouterFactory.h"

#include <unordered_set>
#include <queue>
#include <cmath>
#include <algorithm>
#include <limits>
#include <memory>
#include <string>

using namespace std;

void SteinerTreeSolver::computeSteinerTrees(
	unordered_map<string, vector<PinPad*>>& nets,
	unordered_map<string, NetInfo>& netInfos,
	SteinerTreeMap& forest_roots) {
	forest_roots.clear();
	// 逐个计算每个net的最小生成树，MST
	for (const auto& [netName, pins] : nets) {
		if (pins.empty())  continue;
		if (pins.size() == 1) {
			forest_roots[netName] = make_shared<SteinerNode>(pins[0], true);
			continue;
		}
		// 计算最小生成树（创建独立节点）
		SteinerTreePtr mst_root = computeMST(pins);
		forest_roots[netName] = mst_root;
	}
}

void SteinerTreeSolver::insertSteinerNodes(
	const unordered_map<string, NetInfo>& netsInfos,	//i
	const unordered_map<string, ViaInfo>& viaInfos,		//i
	SteinerTreeMap& forest_roots,						//i,o
	unordered_map<string, PinPad>& preVias				//o
) {
	if (!m_insertSNsOn)
		return;
	m_viaIndex = 0;
	m_netsInfos = &netsInfos;
	m_viaInfos = &viaInfos;
	for (auto& [netName, root] : forest_roots) {
		if (!root || root->children.empty()) continue; // 单节点树没有边，不需要插入斯坦纳点
		m_netName = netName;
		insertSharedSteinerNodes(root, preVias);
	}
}

void SteinerTreeSolver::setFlyLines(const SteinerTreeMap& forest_roots, vector<vector<Line>>& flyLines) {
	flyLines.clear();
	for (const auto& [netName, root] : forest_roots) {
		vector<Line> net_lines;
		generateFlyLinesFromTree(root, net_lines);
		flyLines.emplace_back(net_lines);
	}
}

// 计算最小生成树(Prim算法) - 返回独立创建的节点树
SteinerTreeSolver::SteinerTreePtr SteinerTreeSolver::computeMST(const vector<PinPad*>& original_pins) {
	if (original_pins.empty()) return nullptr;
	int n = (int)original_pins.size();
	vector<bool> visited(n, false);
	vector<double> min_cost(n, numeric_limits<double>::max());
	vector<int> parent(n, -1);

	vector<SteinerTreePtr> nodes;
	for (auto original_pin : original_pins) {
		nodes.emplace_back(make_shared<SteinerNode>(original_pin, true));
	}

	min_cost[0] = 0;

	for (int i = 0; i < n; ++i) {
		// 找到未访问的最小代价节点
		int u = -1;
		double min_val = numeric_limits<double>::max();
		for (int j = 0; j < n; ++j) {
			if (!visited[j] && min_cost[j] < min_val) {
				min_val = min_cost[j];
				u = j;
			}
		}

		if (u == -1) break;
		visited[u] = true;

		// 构建父子关系
		if (parent[u] != -1)
			nodes[parent[u]]->addChild(nodes[u]);

		// 更新邻接节点
		for (int v = 0; v < n; ++v) {
			if (!visited[v]) {
				double dist = getCost(nodes[u], nodes[v]);
				if (dist < min_cost[v]) {
					min_cost[v] = dist;
					parent[v] = u;
				}
			}
		}
	}
	return nodes[0]; // 返回根节点
}

// 从树生成连线
void SteinerTreeSolver::generateFlyLinesFromTree(SteinerTreePtr root, vector<Line>& flyLines) {
	if (!root) return;
	queue<SteinerTreePtr> q;
	unordered_set<SteinerTreePtr> visited;
	q.push(root);
	visited.insert(root);

	while (!q.empty()) {
		SteinerTreePtr current = q.front();
		q.pop();

		// 添加与所有子节点的连线
		for (const auto& child : current->children) {
			if (visited.find(child) == visited.end()) {
				flyLines.emplace_back(current->position, child->position);
				q.push(child);
				visited.insert(child);
			}
		}

		// 添加与父节点的连线（避免重复）
		if (const auto& parent_ptr = current->parent.lock()) {
			if (visited.find(parent_ptr) == visited.end()) {
				flyLines.emplace_back(current->position, parent_ptr->position);
			}
		}
	}
}

double SteinerTreeSolver::getCost(const SteinerTreePtr& node1, const SteinerTreePtr& node2)const {
	double cost_Euclidean = node1->position.distanceTo(node2->position);
	if (m_insertSNsOn) {
		double cost_Via = cost_Euclidean * 0.5;
		for (const auto& shape : node1->pin->shapes) {
			if (node2->pin->shapes.contains(shape.first))
				cost_Via = 0;
		}
		return cost_Euclidean + cost_Via;
	}
	else {
		return cost_Euclidean;
	}
}
void SteinerTreeSolver::insertSharedSteinerNodes(SteinerTreePtr& root, unordered_map<string, PinPad>& preVias) {
	if (!root) return;
	// 1. 收集所有“跨层边”
	vector<TreeEdge> crossLayerEdges;
	collectCrossLayerEdges(root, crossLayerEdges);

	if (crossLayerEdges.empty()) return;

	// 2. 按“min-max layer”分组
	unordered_map<LayerPair, vector<TreeEdge>, LayerPairHash> groupedEdges;
	groupEdgesByLayerPair(crossLayerEdges, groupedEdges);

	// 3. 对每一组插入一个共享 Steiner 点
	for (auto& [layerPair, edges] : groupedEdges)
	{
		vector<vector<TreeEdge>> spatialGroups;
		splitEdgesBySpatialProximity(edges, spatialGroups);

		for (auto& group : spatialGroups)
		{
			if (group.size() >= m_minCrossLayerEdgesThreshold && group.size() <= 4)
			{
				insertOneSharedSteinerNode(layerPair, group, preVias);
			}
		}
	}
}
void SteinerTreeSolver::splitEdgesBySpatialProximity(const vector<TreeEdge>& edges, vector<vector<TreeEdge>>& groups) {
	groups.clear();
	if (edges.empty()) return;
	// 每个 group 对应一个 bbox（用 Line 表示）
	vector<Line> groupBBoxes;
	// 构造一条边的外包矩形（Line 表示）
	auto makeBBox = [](const TreeEdge& e) -> Line
		{
			const Point& p1 = e.first->position;
			const Point& p2 = e.second->position;

			double xmin = std::min(p1.x, p2.x);
			double xmax = std::max(p1.x, p2.x);
			double ymin = std::min(p1.y, p2.y);
			double ymax = std::max(p1.y, p2.y);

			return Line(Point(xmin, ymin), Point(xmax, ymax));
		};
	// 计算两个 bbox 之间的最小距离
	auto bboxDistance = [](const Line& a, const Line& b) -> double
		{
			double dx = std::max({
				a.getPt1().x - b.getPt2().x,
				b.getPt1().x - a.getPt2().x,
				0.0
				});

			double dy = std::max({
				a.getPt1().y - b.getPt2().y,
				b.getPt1().y - a.getPt2().y,
				0.0
				});

			return std::sqrt(dx * dx + dy * dy);
		};
	// 合并两个 bbox
	auto mergeBBox = [](const Line& a, const Line& b) -> Line
		{
			double xmin = std::min(a.getPt1().x, b.getPt1().x);
			double ymin = std::min(a.getPt1().y, b.getPt1().y);
			double xmax = std::max(a.getPt2().x, b.getPt2().x);
			double ymax = std::max(a.getPt2().y, b.getPt2().y);

			return Line(Point(xmin, ymin), Point(xmax, ymax));
		};
	// 主循环
	for (const auto& e : edges)
	{
		Line edgeBBox = makeBBox(e);

		bool assigned = false;

		for (size_t i = 0; i < groups.size(); ++i)
		{
			double dist = bboxDistance(edgeBBox, groupBBoxes[i]);

			if (dist <= MapMinValue)   // 空间接近
			{
				groups[i].push_back(e);
				groupBBoxes[i] = mergeBBox(groupBBoxes[i], edgeBBox);
				assigned = true;
				break;
			}
		}

		// 没有匹配组，新建一个
		if (!assigned)
		{
			groups.emplace_back(vector<TreeEdge>{e});
			groupBBoxes.emplace_back(edgeBBox);
		}
	}
}
void SteinerTreeSolver::collectCrossLayerEdges(SteinerTreePtr root, vector<TreeEdge>& crossLayerEdges) {	//遍历树，找出所有"跨层边"
	if (!root) return;

	queue<SteinerTreePtr> q;
	unordered_set<SteinerTreePtr> visited;

	q.push(root);
	visited.insert(root);

	while (!q.empty()) {
		auto u = q.front();
		q.pop();

		// 遍历 u 的子节点（树边 u -> v）
		for (auto& v : u->children) {
			if (!v) continue;

			if (!hasCommonLayer(u, v)) {
				crossLayerEdges.emplace_back(u, v);
			}

			if (visited.insert(v).second) {
				q.push(v);
			}
		}

		// 父节点方向（保证整棵树都能遍历到）
		if (auto p = u->parent.lock()) {
			if (visited.insert(p).second) {
				q.push(p);
			}
		}
	}
}
bool SteinerTreeSolver::hasCommonLayer(const SteinerTreePtr& a, const SteinerTreePtr& b) {	//判断两个节点是否"层兼容"
	if (!a || !b) return false;
	if (!a->pin || !b->pin) return false;
	const auto& shapesA = a->pin->shapes;
	const auto& shapesB = b->pin->shapes;
	if (shapesA.empty() || shapesB.empty())
		return false;
	// 判断 layer 是否有交集
	for (const auto& [layerA, _] : shapesA) {
		if (shapesB.find(layerA) != shapesB.end()) {
			return true;
		}
	}
	return false;
}
int SteinerTreeSolver::getRepresentativeLayer(const SteinerTreePtr& node)
{//获取一个节点的“代表层”（用于分组）
	assert(node && node->pin);
	assert(!node->pin->shapes.empty());
	return node->pin->shapes.begin()->first;
}
SteinerTreeSolver::LayerPair SteinerTreeSolver::getLayerPairKey(const SteinerTreePtr& u, const SteinerTreePtr& v) {//计算一条边的“层对 key”
	int lu = getRepresentativeLayer(u);
	int lv = getRepresentativeLayer(v);
	if (lu > lv) swap(lu, lv);
	return LayerPair(lu, lv);
}
void SteinerTreeSolver::groupEdgesByLayerPair(const vector<TreeEdge>& crossLayerEdges, unordered_map<LayerPair, vector<TreeEdge>, LayerPairHash>& groupedEdges) {//按层对对边进行分组
	groupedEdges.clear();
	for (const auto& e : crossLayerEdges) {
		const auto& u = e.first;
		const auto& v = e.second;
		LayerPair key = getLayerPairKey(u, v);
		groupedEdges[key].emplace_back(e);
	}
}

void SteinerTreeSolver::insertOneSharedSteinerNode(const LayerPair& layerPair, const vector<TreeEdge>& edges, unordered_map<string, PinPad>& preVias) {//为“一组跨层边”插入一个 Steiner 点（最核心）
	assert(!edges.empty());

	// 1. 计算位置并矫正位置
	Point sum(0, 0);
	for (const auto& [u, v] : edges)
		sum = sum + (u->position + v->position) * 0.5;
	Point steinerPos = sum * (1.0 / edges.size());
	// 微调矫正
	double min_dist = INF;
	Point nearistPos = steinerPos;
    for (const auto& [u, v] : edges) {
		double dist1 = steinerPos.distanceTo(u->position);
		double dist2 = steinerPos.distanceTo(v->position);
		if (dist1 < dist2) {
			if (dist1 < min_dist) {
				min_dist = dist1;
				nearistPos = u->position;
			}
		}
		else {
            if (dist2 < min_dist) {
				min_dist = dist2;
				nearistPos = v->position;
			}
		}
	}
	if(fabs(steinerPos.x - nearistPos.x) < fabs(steinerPos.y - nearistPos.y))
		steinerPos.x = nearistPos.x;
	else
        steinerPos.y = nearistPos.y;


	// 2. 创建 Steiner
	PinPad* steinerPad = createVia(steinerPos, layerPair, preVias);
	if (!steinerPad) return;
	auto steinerNode = make_shared<SteinerNode>(steinerPad, false);

	// 3. 接管跨层边
	bool steinerAttached = false;
	for (const auto& [u, v] : edges) {
		SteinerTreePtr parent = nullptr;
		SteinerTreePtr child = nullptr;

		if (v->parent.lock() == u) {
			parent = u;
			child = v;
		}
		else if (u->parent.lock() == v) {
			parent = v;
			child = u;
		}
		else {
			continue;
		}

		disconnectTreeEdge(parent, child);

		// 只在第一次时，把 Steiner 插到树里
		if (!steinerAttached) {
			parent->addChild(steinerNode);
			steinerAttached = true;
		}

		// 所有 child 都挂到 Steiner
		steinerNode->addChild(child);
	}
}
void SteinerTreeSolver::disconnectTreeEdge(const SteinerTreePtr& u, const SteinerTreePtr& v) {	//断开树中一条边（安全封装）
	if (!u || !v) return;

	// u 是 v 的父
	if (v->parent.lock() == u) {
		u->removeChild(v);
		return;
	}
	// v 是 u 的父
	if (u->parent.lock() == v) {
		v->removeChild(u);
		return;
	}
}

PinPad* SteinerTreeSolver::createVia(const Point& pos, const LayerPair& layerPair, unordered_map<string, PinPad>& preVias) {
	const NetInfo& netInfo = m_netsInfos->at(m_netName);
	const ViaInfo& viaInfo = m_viaInfos->at(netInfo.viaName);

	string viaName = "Steiner_" + to_string(m_viaIndex++);
	preVias.insert(make_pair(viaName, PinPad(pos, viaName, m_netName)));
	PinPad* viaPadPtr = &preVias[viaName];
	int layer1 = layerPair.first;
	int layer2 = layerPair.second;
	for (const int& layer : viaInfo.m_layers) {
		if (layer < layer1)
			layer1 = layer;
		else if (layer > layer2)
			layer2 = layer;
	}
	for (int layer = layer1; layer <= layer2; ++layer) {
		viaPadPtr->addShape(layer, viaInfo.m_radius, {0,0});
	}
	return viaPadPtr;
}