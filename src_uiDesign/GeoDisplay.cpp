#include "GeoDisplay.h"

GeoDisplay::GeoDisplay(QWidget* parent) : QMainWindow(parent) {
	ui.setupUi(this);
	config_init();			//界面初始化（建立基本变量，用于后续其他模块的初始化）
	setupMenuBar();			//1.设置菜单栏
	setupOptionPanel();		//2.设置选项面板
	setupToolBar();			//3.设置工具栏
	setupTabWidget();		//4.设置绘图控件（标签页）
	setup_slots();          //最后，关联信号槽
	srand(time(0));
}

GeoDisplay::~GeoDisplay() {
	m_config->write_config();
	delete m_optionsPanel;
	delete m_optionsDock;
	delete m_config;
}

void GeoDisplay::config_init() {		//配置初始化

	//1.初始界面布局
	// 1.1创建中央部件和主布局
	m_centralWidget = new QWidget(this);
	m_mainLayout = new QVBoxLayout(m_centralWidget);
	// 1.2设置布局属性
	m_mainLayout->setContentsMargins(5, 5, 5, 5);  // 边距
	m_mainLayout->setSpacing(10);                 // 控件间距
	// 1.3将中央部件设置为主窗口的中央部件
	setCentralWidget(m_centralWidget);

	//2.读取配置文件,设置初始文件名
	m_config = new ConfigUI(this);	// 创建配置对象
	m_config->read_config();			// 读取配置信息

	//3.初始界面配置
	ui.actionTool_cmd->setCheckable(true);		//显示工具栏
	bool isToolbarVisible = ui.mainToolBar->isVisible();
	ui.actionTool_cmd->setChecked(isToolbarVisible);

	//4.初始化标签页系统
	m_tabWidget = new QTabWidget(this);
	m_tabWidget->setTabsClosable(true);
	setCentralWidget(m_tabWidget);
}

//最后，关联信号槽
void GeoDisplay::setup_slots() {
	setupMenuBar_slots();               //1.信号槽 - 菜单栏
	setupOptionPanel_slots();			//2.信号槽 - 选项面板
	setupToolBar_slots();               //3.信号槽 - 工具栏（命令）
}

