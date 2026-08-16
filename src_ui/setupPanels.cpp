#include "GeoDisplay.h"
#include "../src_panels/PanelManager.h"
#include "../src_panels/panels/options/OptionsPanel.h"

using namespace std;

//2.设置选项面板（用 PanelManager 统一管理面板挂载）
void GeoDisplay::setupPanels() {
	//1.创建选项面板（继承自 PanelBase，title="选项面板"）
	m_optionsPanel = new OptionsPanel(m_config, this);

	//2.注册到 PanelManager，由其负责创建 QDockWidget 并挂载到主窗口
	PanelManager::instance().registerPanel(m_optionsPanel);
	PanelManager::instance().attachTo(this);

	//3.菜单项 actionOptionPanel 与 PanelManager 创建的 dock 双向绑定
	ui.actionOptionPanel->setCheckable(true);
	if (QDockWidget* dock = PanelManager::instance().dockFor(m_optionsPanel->title())) {
		ui.actionOptionPanel->setChecked(dock->isVisible());
		// action -> dock：勾选=显示
		connect(ui.actionOptionPanel, &QAction::toggled, dock, &QDockWidget::setVisible);
		// dock -> action：dock 显隐状态反向同步到勾选
		connect(dock, &QDockWidget::visibilityChanged, ui.actionOptionPanel, &QAction::setChecked);
	}
}

void GeoDisplay::setupPanels_slots() {
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
