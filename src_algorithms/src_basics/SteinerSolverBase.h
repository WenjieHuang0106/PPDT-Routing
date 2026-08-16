#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "../src_dsn/RoutingNode.h"

// STN 别名统一定义于此（原 MST.h 中的同名别名已移除），SteinerNode 完整定义仍在 MST.h。
// shared_ptr<SteinerNode> 在 SteinerNode 不完整时即可使用，故此处仅前向声明 SteinerNode。
struct SteinerNode;
using STN = std::shared_ptr<SteinerNode>;

// 斯坦纳树求解器抽象基类（Qt-free）。
// 接口签名对接 SteinerTreeSolver 现有公有方法。
class SteinerSolverBase {
public:
	virtual ~SteinerSolverBase() = default;
	using SteinerTreeMap = std::unordered_map<std::string, STN>;
	virtual void computeSteinerTrees(std::unordered_map<std::string, std::vector<PinPad*>>& nets,
		std::unordered_map<std::string, NetInfo>& netInfos,
		SteinerTreeMap& forest_roots) = 0;
	virtual void insertSteinerNodes(const std::unordered_map<std::string, NetInfo>& netsInfos,
		const std::unordered_map<std::string, ViaInfo>& viaInfos,
		SteinerTreeMap& forest_roots,
		std::unordered_map<std::string, PinPad>& preVias) = 0;
	virtual void setFlyLines(const SteinerTreeMap& forest_roots,
		std::vector<std::vector<Line>>& flyLines) = 0;
	virtual void setSteinerNodesOn(bool on) = 0;
};
