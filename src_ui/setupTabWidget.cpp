#include "GeoDisplay.h"
#include <QMessageBox>


//4.设置绘图窗口样式（标签页）
void GeoDisplay::setupTabWidget() {
	//设置样式
	m_tabWidget->setStyleSheet(
		// 设置整个标签栏背景（包括空白区域）
		"QTabBar {"
		"    background-color: #CCCCCC;"  // 灰色背景
		"    border-bottom: 1px solid #999999;" // 底部边框线
		"    min-height: 24px;"           // 设置最小高度（控制整体高度）
		"    height: 24px;"               // 固定高度（确保生效）
		"}"

		// 设置单个标签页样式
		"QTabBar::tab {"
		"    background: #FFFFFF;"      // 未选中标签颜色（白色#FFFFFF）
		"    color: #333333;"           // 文字颜色
		"    border: none;"             // 去掉所有边框（原始设置）
		"    border-right: 1px solid #999999;" // 只保留右边框
		"    padding: 5px 12px;"
		"    margin-right: 0px;"        // 去掉标签间距（边框已作为分隔）
		"    margin-left: -1px;"        // 让标签紧贴（避免边框重叠变粗）
		"}"

		// 选中状态
		"QTabBar::tab:selected {"
		"    background: #DDDDDD;"  // 浅蓝色背景（白色#FFFFFF，浅灰#DDDDDD，蓝色#1565C0，灰蓝#778899）
		"    border-top: 3px solid #778899;"  // 更粗的深蓝条
		"}"

		// 悬停状态
		"QTabBar::tab:hover {"
		"    background: #EEEEEE;"
		"}"

		// 标签栏角落控件（如滚动按钮）背景
		"QTabBar::scroller {"
		"    background: #CCCCCC;"
		"}"

		// 设置QTabWidget的pane样式（可选）
		"QTabWidget::pane {"
		"    border-top: 1px solid #999999;" // 与标签栏边框衔接
		"}"
	);

	//关联信号槽
	connect(m_tabWidget, &QTabWidget::currentChanged, this, &GeoDisplay::onTabChanged);		//监听标签页切换
	connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &GeoDisplay::closeTab);		//监听标签页关闭请求
}

//标签页相关槽函数
//1.三个管理标签页生命周期的函数
void GeoDisplay::addTabWidget(const QString& qFullFileName) {
	//1.新建一个绘图控件
	TabPageBase* newTab = TabPageFactory::instance().create(qFullFileName, m_tabWidget);
	if (!newTab) {
		QMessageBox::warning(this, "无法打开文件",
			QString("没有找到支持该文件类型的标签页: %1").arg(qFullFileName));
		return;
	}
	newTab->pageInit(m_config);
	//2.新建一个标签页,将绘图控件依赖进去
	int tabIndex = m_tabWidget->addTab(newTab, newTab->fileName());
	m_tabWidget->setCurrentIndex(tabIndex);
	m_curPage = newTab;
	//3.连接信号槽(标签页发射信号，选项面板执行槽函数)
	connect(m_curPage, &TabPageBase::requestUpdatePanelUI, m_optionsPanel, &OptionsPanel::updatePanelUI);
}
void GeoDisplay::onTabChanged(int index) {
	//新建标签页时也会触发此函数
	if (index >= 0) {
		m_curPage = qobject_cast<TabPageBase*>(m_tabWidget->widget(index));
		// 安全检查
		if (!m_curPage) {
			qWarning() << "标签页" << index << "不是DiagramWidget类型";
			m_curPage = nullptr;
		}
	}
	else {
		m_curPage = nullptr; // 无标签页时置空
	}
}
void GeoDisplay::closeTab(int index) {
	QWidget* toBeClosed = m_tabWidget->widget(index);
	if (!toBeClosed) return;

	//1.记录是不是当前页 
	bool isCurrent = (m_curPage == toBeClosed);

	//2.从 tab 栏移除并自动销毁 
	m_tabWidget->removeTab(index);          // Qt 会 delete toBeClosed
	delete toBeClosed;
	toBeClosed = nullptr;  // 避免悬空指针

	//3.调整 m_curPage 
	if (isCurrent) {                       // 被关的恰好是当前页
		int remaining = m_tabWidget->count();
		if (remaining > 0) {                // 还有页面
			int newIdx = m_tabWidget->currentIndex(); // 移除后 Qt 已自动调整
			m_curPage = qobject_cast<TabPageBase*>(m_tabWidget->widget(newIdx));
		}
		else {                              // 最后一页也没了
			m_curPage = nullptr;
		}
	}
	//4.更新配置文件信息
	ConfigManager::instance().saveLast(m_config);
}
void GeoDisplay::addAndCloseTabWidget(const QString& qFullFileName) {
	// 1. 创建 TabPage（但不加到 tabWidget）
	TabPageBase* page = TabPageFactory::instance().create(qFullFileName, nullptr);
	if (!page) {
		qWarning() << "无法创建 TabPage:" << qFullFileName;
		return;
	}

	// 2. 初始化
	page->pageInit(m_config);

	// 3. 直接运行算法
	page->pressRunButton();

	// 4. 立即销毁
	delete page;
	page = nullptr;
}
//2.在当前标签页中添加一条数据
void GeoDisplay::actionAddData(QString& qFullFileName, const int readType) {
	//BasicData* curData = m_curPage->m_data;
	//curData->acquired_ = m_dp.readFile<BasicData>(qFullFileName, readType, curData);
	//if (curData->acquired_) {
	//	m_curTap->m_data_type = readType;
	//	curData->runFinished_ = false;
	//	m_curTap->initViewTransform();		//需触发绘图初始化
	//}
	//else {
	//	curData->acquired_ = true;
	//	QMessageBox::warning(this, "错误", "新增文件解析失败");
	//}
}