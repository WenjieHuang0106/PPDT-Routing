#include "GeoDisplay.h"

using namespace std;

//2.设置选项面板
void GeoDisplay::setupOptionPanel() {
	m_optionsPanel = new OptionsPanel(m_config, this);

	// 创建停靠窗口并设置选项面板
	m_optionsDock = new QDockWidget("选项面板", this);
	m_optionsDock->setWidget(m_optionsPanel);
	m_optionsDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

	// 将停靠窗口添加到主窗口左侧
	addDockWidget(Qt::LeftDockWidgetArea, m_optionsDock);

	// 添加一个菜单项来控制面板的显示/隐藏
	ui.actionOptionPanel->setCheckable(true);
	ui.actionOptionPanel->setChecked(true);
}

void GeoDisplay::setupOptionPanel_slots() {
	//选项面板发射信号，标签页响应信号
	connect(m_optionsPanel, &OptionsPanel::update_request_signal, this, [this]() {			// 请求重新绘图
		if (m_curPage != nullptr)
			m_curPage->onConfigChanged();
		});
	connect(m_optionsPanel, &OptionsPanel::tranform_init_request_signal, this, [this]() {	// 请求重新绘图，更新转化矩阵
		if (m_curPage != nullptr)
			m_curPage->tranformChanged();
		});
	connect(m_optionsPanel, &OptionsPanel::pressMSTReset, this, [this]() {					// MST重置，根据preVia重新生成斯坦纳树
		if (m_curPage != nullptr)
			m_curPage->onConfigChanged(2);
		});
	connect(m_optionsPanel, &OptionsPanel::open_res_request_signal, this, [this]() {		// 打开结果数据进行查看
		action_open();
		});
	connect(m_optionsPanel, &OptionsPanel::reset_request_signal, this, [this]() {			// 复位重置
		m_curPage->action_reset();
		});
	connect(m_optionsPanel, &OptionsPanel::random_pin_request_signal, this, [this]() {		// 重新生成随机pin
		m_curPage->action_random_pin();
		});
	connect(m_optionsPanel, &OptionsPanel::pin_decrease_request_signal, this, [this]() {	// pin数量减少
		m_curPage->pin_decrease();
		});
	connect(m_optionsPanel, &OptionsPanel::pin_increase_request_signal, this, [this]() {	// pin数量增加
		m_curPage->action_pin_increase();
		});
	connect(m_optionsPanel, &OptionsPanel::save_case_request_signal, this, [this]() {		// 保存case
		m_curPage->action_save_case();
		});
	connect(m_optionsPanel, &OptionsPanel::save_image_request_signal, this, [this]() {		// 保存图片
		m_curPage->action_save_image();
		});
	connect(m_optionsPanel, &OptionsPanel::write_pre_request_signal, this, [this]() {		// 保存预处理结果
		m_curPage->action_write_pre();
		});
	connect(m_optionsPanel, &OptionsPanel::write_res_request_signal, this, [this]() {		// 在预处理结果后面追加结果
		m_curPage->action_write_res();
		});
   
	connect(m_optionsPanel, &OptionsPanel::PPTT_run_request_signal, this, [this]() {		// 运行PPDT
		m_curPage->pressRunButton();
		});
	connect(m_optionsPanel, &OptionsPanel::poly_run_request_signal, this, [this]() {		// 运行PPDT
		m_curPage->pressRunButton();
		});
}

