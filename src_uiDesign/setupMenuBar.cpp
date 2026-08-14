#include "GeoDisplay.h"
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include "../src_baseClasses/TabPage.h"

using namespace std;

//1.设置菜单栏
void GeoDisplay::setupMenuBar() {
	;
}
void GeoDisplay::setupMenuBar_slots() {
	//文件
	connect(ui.actionOpen, &QAction::triggered, this, &GeoDisplay::action_open);				//打开
	connect(ui.actionAdd, &QAction::triggered, this, [this]() {action_add(); });				//新增
	connect(ui.actionSave, &QAction::triggered, this, [this]() {m_curPage->action_save_image(); });		//保存
	connect(ui.actionOutput, &QAction::triggered, this, [this]() {action_output(0); });			//输出
	connect(ui.actionRun, &QAction::triggered, this, [this]() {action_run(); });				//执行算法
	connect(ui.actionRun_Batch, &QAction::triggered, this, [this]() {action_runBatch(); });		//执行算法（批处理）

	//编辑

	//窗口（显示，隐藏各种窗口）
	connect(ui.actionOptionPanel, &QAction::toggled, m_optionsDock, &QDockWidget::setVisible);			//显示/隐藏选项面板
	connect(ui.actionTool_cmd, &QAction::toggled, ui.mainToolBar, &QToolBar::setVisible);				//显示/隐藏工具栏
	connect(ui.mainToolBar, &QToolBar::visibilityChanged, ui.actionTool_cmd, &QAction::setChecked);		//工具栏显示状态改变时，更新菜单栏（双向绑定）
}

void GeoDisplay::action_open() {
	const QString fileFilter =
		"全部 (*);;"
		"DSN文件 (*.dsn *.DSN);;"
		"文本文件 (*.txt);;"
		"图片 (*.png *.jpg)";

	// 一次只能打开一个文件
	//QString file = QFileDialog::getOpenFileName(
	//	this,
	//	"打开文件",
	//	m_config->m_qFilePath,
	//	fileFilter
	//);
	//if (file.isEmpty())
	//	return;
	//addTabWidget(file);


	/**/
	//一次打开多个文件
	QStringList files = QFileDialog::getOpenFileNames(
		this,
		"打开文件",
		m_config->m_qFilePath,
		fileFilter
	);

	if (files.isEmpty())
	return;
	for (const QString& file : files) {
		addTabWidget(file);
	}
	/**/
}

void GeoDisplay::action_add() {
	if (m_tabWidget->count() == 0) {
		action_open();
	}
	else {
		/*
		//1.读取文件,获取文件名称,更新配置信息
		QString qFullFileName = QFileDialog::getOpenFileName(this, "打开文件", m_config->m_qFilePath, "*");
		if (qFullFileName.isEmpty()) { qDebug() << "Failed to read data!"; return; }
		QFileInfo fileInfo(qFullFileName);
		m_config->m_qFilePath = fileInfo.absolutePath();
		m_config->m_qFileName = fileInfo.fileName();
		//2.根据文件名称,确定文件解析方案
		int read_file_type = -1;
		QString qfileName_lower = m_config->m_qFileName.toLower();
		if (qfileName_lower.endsWith(".png") || qfileName_lower.endsWith(".jpg")) {
			read_file_type = 0;		//解析图片形式的地图文件
		}
		else if (qfileName_lower.endsWith(".txt")) {
			if (qfileName_lower.startsWith("post")) {
				read_file_type = 1;	//解析后处理文件
			}
			else if (qfileName_lower.startsWith("map")) {
				read_file_type = 2;	//解析文本形式的地图文件
			}
		}
		//3.无法解析的类型
		if (read_file_type == 1 && m_curTap->m_data_type == 1) {	//两者都为后处理文件
			actionAddData(qFullFileName, read_file_type);
		}
		else {
			QMessageBox::warning(this, "错误!", "添加的数据类型不匹配");
			return;
		}
		*/
	}
}
void GeoDisplay::action_output(int type) {
	//type取值：0，输出后处理之后的数据
	switch (type) {
	case 0:
		m_curPage->action_write_res();
		break;
	case 1:
		break;
	default:
		break;
	}
}
void GeoDisplay::action_run() {
	if (m_curPage)
		m_curPage->pressRunButton();
}
void GeoDisplay::action_runBatch() {
	const QString fileFilter =
		"全部 (*);;"
		"DSN文件 (*.dsn *.DSN);;"
		"文本文件 (*.txt);";

	QStringList files = QFileDialog::getOpenFileNames(
		this,
		"批量运行文件",
		m_config->m_qFilePath,
		fileFilter
	);

	if (files.isEmpty())
		return;

	for (const QString& file : files) {
		addAndCloseTabWidget(file);
	}
	QMessageBox::information(this, "批处理完成", "所有文件已处理完成。");
}
