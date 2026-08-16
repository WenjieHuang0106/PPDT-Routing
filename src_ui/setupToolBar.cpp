#include "GeoDisplay.h"
using namespace std;


//3.设置工具栏（命令）
void GeoDisplay::setupToolBar() {
	ui.mainToolBar->setWindowTitle(tr("命令"));
	//工具栏命令
	ui.mainToolBar->addAction(ui.actionOpen);
	ui.mainToolBar->addAction(ui.actionAdd);
	ui.mainToolBar->addAction(ui.actionClear);
	ui.mainToolBar->addAction(ui.actionRun);
	ui.mainToolBar->addAction(ui.actionRun_Batch);
	ui.mainToolBar->addAction(ui.actionOutput);
}

void GeoDisplay::setupToolBar_slots() {
	;
}
//void GeoDisplay::refreshStateLabel()
//{
//	//状态栏展示鼠标位置
//	QString str = "(" + QString::number(mouse_x) + "," + QString::number(mouse_y) + ")";
//	statusLabel->setText(state_info + str);
//}