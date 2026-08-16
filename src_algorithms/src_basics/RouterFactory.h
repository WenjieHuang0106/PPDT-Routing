#pragma once
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>

class RouterBase;
class SteinerSolverBase;

// 路由器工厂：按名称注册/创建 RouterBase 实例（Qt-free）。
class RouterFactory {
public:
	using CreatorFunc = std::function<std::unique_ptr<RouterBase>()>;
	static RouterFactory& instance();
	void registerCreator(const std::string& name, CreatorFunc creator);
	std::unique_ptr<RouterBase> create(const std::string& name);
private:
	RouterFactory() = default;
	std::unordered_map<std::string, CreatorFunc> m_creators;
};

// 斯坦纳求解器工厂：按名称注册/创建 SteinerSolverBase 实例（Qt-free）。
class SteinerFactory {
public:
	using CreatorFunc = std::function<std::unique_ptr<SteinerSolverBase>()>;
	static SteinerFactory& instance();
	void registerCreator(const std::string& name, CreatorFunc creator);
	std::unique_ptr<SteinerSolverBase> create(const std::string& name);
private:
	SteinerFactory() = default;
	std::unordered_map<std::string, CreatorFunc> m_creators;
};
