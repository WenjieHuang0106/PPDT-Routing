#include "../src_baseClasses/TabPage.h"

//1.包含所需头文件
#include "TabPage_dsn/TabPage_dsn.h"		//(2)dsn类PCB文件

namespace {
	// 2.定义构造函数
	static auto fun_dsn = [](	//(3)dsn类PCB文件
		const QString& qFullName, const QString& qFilePath, const QString& qFileName, QWidget* parent) {
			return new TabPage_dsn(qFullName, qFilePath, qFileName, parent);
		};

	struct TabPageRegister {
		TabPageRegister() {
			TabPageFactory& factory = TabPageFactory::instance();
			std::vector<QString> suffixes;
			suffixes = { "dsn" };									//(3)dsn类PCB文件
			for (const auto& suffix : suffixes) {
				factory.registerCreator(suffix, fun_dsn);
			}


		}
	};
	TabPageRegister registrar_grid;
}
