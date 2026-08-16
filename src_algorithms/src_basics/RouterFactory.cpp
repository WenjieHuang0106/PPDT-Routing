#include "RouterFactory.h"
#include "RouterBase.h"
#include "SteinerSolverBase.h"

RouterFactory& RouterFactory::instance() {
	static RouterFactory inst;
	return inst;
}

void RouterFactory::registerCreator(const std::string& name, CreatorFunc creator) {
	m_creators[name] = std::move(creator);
}

std::unique_ptr<RouterBase> RouterFactory::create(const std::string& name) {
	auto it = m_creators.find(name);
	if (it == m_creators.end())
		return nullptr;
	return it->second();
}

SteinerFactory& SteinerFactory::instance() {
	static SteinerFactory inst;
	return inst;
}

void SteinerFactory::registerCreator(const std::string& name, CreatorFunc creator) {
	m_creators[name] = std::move(creator);
}

std::unique_ptr<SteinerSolverBase> SteinerFactory::create(const std::string& name) {
	auto it = m_creators.find(name);
	if (it == m_creators.end())
		return nullptr;
	return it->second();
}
