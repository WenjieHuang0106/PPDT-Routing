#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <cassert>
#include <stack>

#include "RoutingNode.h"
#include "../src_basics/SteinerSolverBase.h"
#include "../src_basics/RouterFactory.h"
// SteinerNode 前向声明与 STN 别名已移至 SteinerSolverBase.h，此处通过 include 获得

struct SteinerNode : public std::enable_shared_from_this<SteinerNode> {
	PinPad* pin;  // 指向独立创建的PinPad对象
	Point position;
	double cost;
	weak_ptr<SteinerNode> parent;  // 使用weak_ptr避免循环引用
	vector<shared_ptr<SteinerNode>> children;  // 使用shared_ptr
	bool is_original_pin;  // 标记是否是原始输入点

	SteinerNode(PinPad* p, bool original = false, double c = 0,
		STN par = nullptr)
		: pin(p), cost(c), is_original_pin(original) {
		assert(p != nullptr && "PinPad must not be null for SteinerNode");
		if (p) {
			position = p->pos;  // 使用PinPad的pos作为位置
		}
		if (par) {
			parent = par;
		}
	}

	// 安全的析构函数
	~SteinerNode() {}

	// 添加子节点
	void addChild(STN child) {
		if (!child || child.get() == this) 
			return;
		for (auto& ch : children) 
			if (ch == child)
				return;
		if (auto oldParent = child->parent.lock()) 
			oldParent->removeChild(child);
		children.emplace_back(child);
		child->parent = weak_from_this();
	}

	// 移除子节点
	void removeChild(STN child) {
		if (!child)
			return;
		auto it = std::find(children.begin(), children.end(), child);
		if (it != children.end()) {
			children.erase(it);
			child->parent.reset();
		}
	}

	// 断开与父节点的连接
	void disconnectFromParent() {
		if (auto parent_ptr = parent.lock()) {
			parent_ptr->removeChild(shared_from_this());
		}
	}

	void getAdjacentNodes(const Point& ignorePos, vector<shared_ptr<SteinerNode>>& neib) {
		neib.clear();
		if (auto p = parent.lock()) {
			if (!(p->position == ignorePos)) {
				neib.emplace_back(p);
			}
		}
		for (const auto& ch : children) {
			if (!ch) continue;
			if (!(ch->position == ignorePos)) {
				neib.emplace_back(ch);
			}
		}
	}

	void changeTopology(const STN& node) {
		if (!node || node.get() == this)
			return;
		if (auto oldParent = node->parent.lock()) {
			oldParent->removeChild(node);
			node->parent = weak_from_this();
		}
		this->addChild(node);
	}
	bool changeTopologyPC(const STN& oldNode, const STN& newNode) {
		if (!oldNode || !newNode) {
			std::cerr << "Error: invalid nodes\n";
			return false;
		}
		bool thisIsChild = false;
		if (this->parent.lock() == oldNode)
			thisIsChild = true;
		else if (oldNode->parent.lock().get() == this)
			thisIsChild = false;
		else {
			std::cerr << "Error: not direct parent-child\n";
			return false;
		}
		if (thisIsChild) {
			oldNode->removeChild(shared_from_this());
			newNode->addChild(shared_from_this());
		}
		else {
			std::vector<STN> path;
			STN cur = newNode;
			while (cur && cur != oldNode) {
				path.push_back(cur);
				cur = cur->parent.lock();
			}
			if (cur != oldNode) {
				std::cerr << "Error: newNode not in subtree\n";
				return false;
			}
			path.push_back(oldNode);
			for (size_t i = 0; i + 1 < path.size(); ++i) {
				STN child = path[i];
				STN parent = path[i + 1];
				parent->removeChild(child);
				child->addChild(parent);
			}
			removeChild(oldNode);
			addChild(newNode);
		}
		return true;
	}
	void collectSubtreeDown(vector<STN>& result) {
		result.clear();
		std::stack<STN> stk;
		stk.push(shared_from_this());
		while (!stk.empty()) {
			STN cur = stk.top();
			stk.pop();
			result.emplace_back(cur);
			for (auto& ch : cur->children) {
				if (!ch) continue;
				stk.push(ch);
			}
		}
	}
	void collectSubtreeUp(vector<STN>& result, STN ignoreNode) {
		result.clear();
		STN root = shared_from_this();
		while (STN p = root->parent.lock())
			root = p;
		std::stack<STN> stk;
		stk.push(root);
		while (!stk.empty()) {
			STN cur = stk.top();
			stk.pop();
			result.emplace_back(cur);
			for (auto& ch : cur->children) {
				if (!ch) continue;
				if (ch == ignoreNode) continue; // 跳过 this 的子树
				stk.push(ch);
			}
		}
	}
};

class SteinerTreeSolver : public SteinerSolverBase {
public:
	using SteinerTreePtr = STN;
	using SteinerTreeMap = std::unordered_map<std::string, SteinerTreePtr>;
	using TreeEdge = std::pair<SteinerTreePtr, SteinerTreePtr>;
	using LayerPair = std::pair<int, int>;
	int m_viaIndex = 0;
	string m_netName;
	const unordered_map<string, NetInfo>* m_netsInfos = nullptr;
	const unordered_map<string, ViaInfo>* m_viaInfos = nullptr;
	int m_minCrossLayerEdgesThreshold = 2;
	bool m_insertSNsOn = true;

	SteinerTreeSolver() {};

	~SteinerTreeSolver() {}

	// 计算所有net的斯坦纳树
	void computeSteinerTrees(std::unordered_map<std::string, std::vector<PinPad*>>& nets, std::unordered_map<std::string, NetInfo>& netInfos, SteinerTreeMap& forest_roots) override;
	void insertSteinerNodes(const unordered_map<string, NetInfo>& netsInfos, const unordered_map<string, ViaInfo>& viaInfos, SteinerTreeMap& forest_roots, unordered_map<string, PinPad>& preVias) override;
	void setFlyLines(const SteinerTreeMap& forest_roots, std::vector<std::vector<Line>>& flyLines) override;

	void setSteinerNodesOn(bool steinerNodes) override { m_insertSNsOn = steinerNodes; };

private:
	SteinerTreePtr computeMST(const std::vector<PinPad*>& original_pins);
	void generateFlyLinesFromTree(SteinerTreePtr root, std::vector<Line>& flyLines);

	struct LayerPairHash {
		size_t operator()(const LayerPair& p) const {
			return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
		}
	};
private:
	double getCost(const SteinerTreePtr& node1, const SteinerTreePtr& node2) const;
	void insertSharedSteinerNodes(SteinerTreePtr& root, unordered_map<string, PinPad>& preVias);
	void splitEdgesBySpatialProximity(const vector<TreeEdge>& edges, vector<vector<TreeEdge>>& groups);
	void collectCrossLayerEdges(SteinerTreePtr root, std::vector<TreeEdge>& crossLayerEdges);	//遍历树，找出所有"跨层边"
	bool hasCommonLayer(const SteinerTreePtr& a, const SteinerTreePtr& b);						//判断两个节点是否"层兼容"
	int getRepresentativeLayer(const SteinerTreePtr& node);										//获取一个节点的“代表层”（用于分组）
	LayerPair getLayerPairKey(const SteinerTreePtr& u, const SteinerTreePtr& v);					//计算一条边的“层对 key”
	//按层对对边进行分组（共享的关键）
	void groupEdgesByLayerPair(const vector<TreeEdge>& crossLayerEdges, unordered_map<LayerPair, vector<TreeEdge>, LayerPairHash>& groupedEdges);
	//为“一组跨层边”插入一个 Steiner 点（最核心）
	void insertOneSharedSteinerNode(const LayerPair& layerPair, const std::vector<TreeEdge>& edges, unordered_map<string, PinPad>& preVias);
	void disconnectTreeEdge(const SteinerTreePtr& u, const SteinerTreePtr& v);					//断开树中一条边（安全封装）
	PinPad* createVia(const Point& pos, const LayerPair& layerPair, unordered_map<string, PinPad>& preVias);
};

// 自注册到 SteinerFactory
namespace detail {
	struct MSTRegistrar {
		MSTRegistrar() {
			SteinerFactory::instance().registerCreator("MST",
				[]() { return std::unique_ptr<SteinerSolverBase>(new SteinerTreeSolver()); });
		}
	};
	static MSTRegistrar g_mstRegistrar;
}


