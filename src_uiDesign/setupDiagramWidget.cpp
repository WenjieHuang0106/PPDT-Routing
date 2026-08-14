#include "GeoDisplay.h"
#include <QMessageBox>

//4.设置绘图窗口（标签页）
void GeoDisplay::setupDiagramWidget() {
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
void GeoDisplay::addDiagramTab(const QString& qFullFileName, const int readType, QWidget* parent) {
	BasicData* newData = new BasicData();
	newData->acquired_ = false;
	m_dp.setData(newData);		//更新数据处理器
	newData->acquired_ = m_dp.readFile(qFullFileName, readType);		//解析文件，将数据存入data对象中，作为前后端的桥梁
	if (newData->acquired_) {
		//1.新建控件
		//1.1新建绘图控件,设置控件相关参数
		DiagramWidget* newTab = new DiagramWidget(newData, m_config, parent);		//会触发绘图
		newTab->m_qFilePath = m_dp.m_qFilePath;
		newTab->m_qFileName = m_dp.m_qFileName;
		newTab->m_data_type = readType;
		newData->runFinished_ = false;		//newTab->m_data 与 newData实际上是一个对象

		//2.新建一个标签页,将绘图控件依赖进去
		int tabIndex = m_tabWidget->addTab(newTab, newTab->m_qFileName);	//新建标签页
		m_tabWidget->setCurrentIndex(tabIndex);		//设置当前标签页编号
		m_tabWidget->setTabsClosable(true);			//启用关闭按钮
		//3.添加新标签页并设为当前页
		m_curTap = newTab;
		//4.设置当前画图控件的其他参数
	}
	else {
		delete newData;
		newData = nullptr;
		QMessageBox::warning(this, "错误", "文件解析失败");
	}
	m_config->write_config();
}
void GeoDisplay::onTabChanged(int index) {
	//新建标签页时也会触发此函数
	if (index >= 0) {
		m_curTap = qobject_cast<DiagramWidget*>(m_tabWidget->widget(index));
		// 安全检查
		if (!m_curTap) {
			qWarning() << "标签页" << index << "不是DiagramWidget类型";
			m_curTap = nullptr;
		}
	}
	else {
		m_curTap = nullptr; // 无标签页时置空
	}
}
void GeoDisplay::closeTab(int index) {
	if (QWidget* tab = m_tabWidget->widget(index)) {
		DiagramWidget* diagram = qobject_cast<DiagramWidget*>(tab);
		// 删除数据（如果是当前标签页，需要额外处理）
		if (diagram && diagram->m_data) {
			if (diagram == m_curTap) {// 如果关闭的是当前标签页，需要清空指针
				m_curTap = nullptr;
			}
			delete diagram->m_data;
			diagram->m_data = nullptr;
		}
		// 移除标签页
		m_tabWidget->removeTab(index);
		tab->deleteLater();
		if (m_tabWidget->count() == 0) {// 如果没有标签页了，清空指针
			m_curTap = nullptr;
		}
	}
}
//2.在当前标签页中添加一条数据
void GeoDisplay::actionAddData(QString& qFullFileName, const int readType) {
	BasicData* curData = m_curTap->m_data;
	curData->acquired_ = false;
	m_dp.setData(curData);
	curData->acquired_ = m_dp.readFile(qFullFileName, readType);
	if (curData->acquired_) {
		m_curTap->m_data_type = readType;
		curData->runFinished_ = false;
		m_curTap->initViewTransform();		//需触发绘图初始化
	}
	else {
		curData->acquired_ = true;
		QMessageBox::warning(this, "错误", "新增文件解析失败");
	}
}