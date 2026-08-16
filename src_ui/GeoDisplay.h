#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_GeoDisplay.h"
#include <QDockWidget>
#include <QVBoxLayout>
#include <QLabel>
#include "../src_config/AppConfig.h"
#include "../src_config/ConfigManager.h"
#include "../src_panels/PanelManager.h"
#include "../src_panels/panels/options/OptionsPanel.h"
#include "../src_paint/TabPageBase.h"

class OptionsPanel;

class GeoDisplay : public QMainWindow {
	Q_OBJECT

public:
	GeoDisplay(QWidget* parent = nullptr);
	~GeoDisplay();

public:
	QTabWidget* m_tabWidget;		//标签页控件,用于管理所有标签页
	TabPageBase* m_curPage;			//当前激活的标签页

private:
	Ui::GeoDisplayClass ui;
	QVBoxLayout* m_mainLayout;  // 主布局
	QWidget* m_centralWidget;   // 中央部件（必须通过QWidget中转布局）
	AppConfig* m_config;

private: //几大模块
	void setupMenuBar();            //1.设置菜单栏
	void setupPanels();             //2.设置选项面板(用 PanelManager)
	void setupToolBar();			//3.设置工具栏（命令）
	void setupTabWidget();			//4.设置绘图窗口（标签页）
	void config_init();				//配置初始化
	void setup_slots();             //最后，关联信号槽

private: // 成员变量
	//2.侧边选项面板
	OptionsPanel* m_optionsPanel;	// 选项面板（由 PanelManager 负责挂载 dock）

	//3.工具栏（命令）

	//5.状态栏
	QLabel* statusLabel;


private: // 子函数
	//1.菜单栏
	void setupMenuBar_slots();
	void action_open();				//打开
	void action_add();				//新增
	void action_output(int type);	//输出
	void action_run();				//执行算法
	void action_runBatch();			//批量执行算法

	//2.侧边选项面板的信号转发
	void setupPanels_slots();
	//3.工具栏（命令）
	void setupToolBar_slots();
	//4.工具栏（绘图）
	void addTabWidget(const QString& qFullFileName);		//新建标签页
	void onTabChanged(int index);	//标签页切换
	void closeTab(int index);		//关闭标签页
	void addAndCloseTabWidget(const QString& qFullFileName);
	void actionAddData(QString& qFullFileName, const int readType);


	//5.状态栏
	//void refreshStateLabel();

};
