#include "OptionsPanel.h"
#include <QLabel>
#include <QGroupBox>
#include <QSlider>

#include <QFileDialog>   //保存文件
#include <QMessageBox>
#include <QButtonGroup>
#include <QRadioButton>


void OptionsPanel::updatePanelUI(int size = 0) {
	ui.treeIndex->blockSignals(true);		// 阻塞信号
	//后处理模式
	ui.treeIndex->clear();
	ui.treeIndex->addItem("All", -1);
	for (int i = 0; i < size; i++)
		ui.treeIndex->addItem(QString::number(i), i);
	ui.treeIndex->blockSignals(false);		// 恢复信号
	int targetIndex = ui.treeIndex->findData(m_config->m_showTreeIndex);
	ui.treeIndex->setCurrentIndex(targetIndex);
}

OptionsPanel::OptionsPanel(ConfigUI* conf, QWidget* parent) : QWidget(parent) {
	ui.setupUi(this);
	m_config = conf;
	styleInit();
	setPanelUI();
	setup_panelSlots();
}

OptionsPanel::~OptionsPanel() {}

void OptionsPanel::styleInit() {

}

void OptionsPanel::setPanelUI() {
	//后处理模式
	ui.PostMode->addItem("None", 0);
	ui.PostMode->addItem("Off", 1);
	ui.PostMode->addItem("Minimal", 2);
	ui.PostMode->addItem("Smooth", 3);
	ui.PostMode->addItem("Full", 4);

	//网格类型: 0无网格，1线形网格，2点形网格
	ui.showGridType->addItem("Hide", 0);
	ui.showGridType->addItem("Line", 1);
	ui.showGridType->addItem("Point", 2);

	//推挤模式
	ui.pushRunMode->addItem("Off", 0);
	ui.pushRunMode->addItem("Block", 1);
}

void OptionsPanel::setup_panelSlots() {
	//1.按钮信号
	connect(ui.openResBt, &QPushButton::clicked, this, [this]() {			// 打开Ren文件，用于查看
		emit open_res_request_signal();
		});
	connect(ui.resetBt, &QPushButton::clicked, this, [this]() {				// 复位重置
		emit reset_request_signal();
		});
	connect(ui.randPinBt, &QPushButton::clicked, this, [this]() {			// 随机pin
		emit random_pin_request_signal();
		});
	connect(ui.pinDecreaseBt, &QPushButton::clicked, this, [this]() {		// 减少pin
		emit pin_decrease_request_signal();
		});
	connect(ui.pinIncreaseBt, &QPushButton::clicked, this, [this]() {		// 增加pin
		emit pin_increase_request_signal();
		});
	connect(ui.saveCaseBt, &QPushButton::clicked, this, [this]() {			// 保存case
		emit save_case_request_signal();
		});
	connect(ui.saveImageBt, &QPushButton::clicked, this, [this]() {			// 保存图片
		emit save_image_request_signal();
		});
	connect(ui.writePreBt, &QPushButton::clicked, this, [this]() {			// 保存预处理结果
		emit write_pre_request_signal();
		});
	connect(ui.addResBt, &QPushButton::clicked, this, [this]() {			// 追加保存结果
		emit write_res_request_signal();
		});

	//2.视图选项
	ui.checkFlipTD->setChecked(m_config->m_flip_up_down);
	connect(ui.checkFlipTD, &QCheckBox::checkStateChanged, this, [this]() {			//01 上下翻转
		m_config->m_flip_up_down = ui.checkFlipTD->checkState() == Qt::Checked;
		paintChanged(1);
		});
	ui.checkFlipLR->setChecked(m_config->m_flip_left_right);
	connect(ui.checkFlipLR, &QCheckBox::checkStateChanged, this, [this]() {			//02 左右翻转
		m_config->m_flip_left_right = ui.checkFlipLR->checkState() == Qt::Checked;
		paintChanged(1);
		});
	ui.checkRotate90->setChecked(m_config->m_rotate_90);
	connect(ui.checkRotate90, &QCheckBox::checkStateChanged, this, [this]() {		//03 旋转90度
		m_config->m_rotate_90 = ui.checkRotate90->checkState() == Qt::Checked;
		paintChanged(1);
		});

	//3.算法选项
	ui.checkPreViaAlct->setChecked(m_config->m_preViaAlctOn);
	connect(ui.checkPreViaAlct, &QCheckBox::checkStateChanged, this, [this]() {		//11 启用过孔预分配
		m_config->m_preViaAlctOn = ui.checkPreViaAlct->checkState() == Qt::Checked;
		paintChanged(2);
		});
	ui.checkGNDRoute->setChecked(m_config->m_GNDRouteOn);
	connect(ui.checkGNDRoute, &QCheckBox::checkStateChanged, this, [this]() {      //12 GND特殊布线
		m_config->m_GNDRouteOn = ui.checkGNDRoute->checkState() == Qt::Checked;
		});
	ui.checkVCCRoute->setChecked(m_config->m_VCCRouteOn);
	connect(ui.checkVCCRoute, &QCheckBox::checkStateChanged, this, [this]() {      //13 VCC特殊布线
		m_config->m_VCCRouteOn = ui.checkVCCRoute->checkState() == Qt::Checked;
		});
	ui.checkDiffRoute->setChecked(m_config->m_diffRouteOn);
	connect(ui.checkDiffRoute, &QCheckBox::checkStateChanged, this, [this]() {     //14 差分布线
		m_config->m_diffRouteOn = ui.checkDiffRoute->checkState() == Qt::Checked;
		});
	ui.check4_8Tree->setChecked(m_config->m_4_8Tree);
	connect(ui.check4_8Tree, &QCheckBox::checkStateChanged, this, [this]() {		//15 4/8方向搜索树
		m_config->m_4_8Tree = ui.check4_8Tree->checkState() == Qt::Checked;
		});
	ui.checkAutoPush->setChecked(m_config->m_autoPush);
	connect(ui.checkAutoPush, &QCheckBox::checkStateChanged, this, [this]() {		//16 自动推线，几何重构
		m_config->m_autoPush = ui.checkAutoPush->checkState() == Qt::Checked;
		});
	ui.checkWriteExpRes->setChecked(m_config->m_autoWriteExpResult);
	connect(ui.checkWriteExpRes, &QCheckBox::checkStateChanged, this, [this]() {		//16 自动推线，几何重构
		m_config->m_autoWriteExpResult = ui.checkWriteExpRes->checkState() == Qt::Checked;
		});

	//4.显示选项
	ui.showObs1->setChecked(m_config->m_showObs1);
	connect(ui.showObs1, &QCheckBox::checkStateChanged, this, [this]() {		//21 显示第1层障碍物
		m_config->m_showObs1 = ui.showObs1->checkState() == Qt::Checked;
		paintChanged(0);
		});
	ui.showObs2->setChecked(m_config->m_showObs2);
	connect(ui.showObs2, &QCheckBox::checkStateChanged, this, [this]() {		//22 显示第2层障碍物
		m_config->m_showObs2 = ui.showObs2->checkState() == Qt::Checked;
		paintChanged(0);
		});
	ui.showFlyLines->setChecked(m_config->m_showFlyLines);
	connect(ui.showFlyLines, &QCheckBox::toggled, this, [this](bool checked) {	//23 显示飞线
		m_config->m_showFlyLines = checked;
		paintChanged(0);
		});
	ui.showPahts->setChecked(m_config->m_showPahts);
	connect(ui.showPahts, &QCheckBox::checkStateChanged, this, [this]() {		//24 显示走线结果
		m_config->m_showPahts = ui.showPahts->checkState() == Qt::Checked;
		paintChanged(0);
		});
	ui.showPlanningPts->setChecked(m_config->m_showPPs);
	connect(ui.showPlanningPts, &QCheckBox::checkStateChanged, this, [this]() {	//25 显示规划点
		m_config->m_showPPs = ui.showPlanningPts->checkState() == Qt::Checked;
		paintChanged(0);
		});
	ui.showVias->setChecked(m_config->m_show_vias);
	connect(ui.showVias, &QCheckBox::checkStateChanged, this, [this]() {		//26 显示Vias
		m_config->m_show_vias = ui.showVias->checkState() == Qt::Checked;
		paintChanged(0);
		});
	ui.showPinCenters->setChecked(m_config->m_showPinCenters);
	connect(ui.showPinCenters, &QCheckBox::checkStateChanged, this, [this]() {    //27 显示Pin中心
		m_config->m_showPinCenters = ui.showPinCenters->checkState() == Qt::Checked;
		paintChanged(0);
		});


	switch (m_config->m_directionOp) {
	case 1: ui.radioButtonD4->setChecked(true); break;	// 四方向
	case 2: ui.radioButtonD8->setChecked(true); break;	// 八方向
	default:ui.radioButtonD0->setChecked(true); break;	// 无方向约束
	}
	auto updateDirectionConfig = [this]() {
		if (ui.radioButtonD0->isChecked()) {
			m_config->m_directionOp = 0;
		}
		else if (ui.radioButtonD4->isChecked()) {
			m_config->m_directionOp = 1;
		}
		else if (ui.radioButtonD8->isChecked()) {
			m_config->m_directionOp = 2;
		}
		paintChanged(0);
		};
	connect(ui.radioButtonD0, &QRadioButton::toggled, this, updateDirectionConfig);
	connect(ui.radioButtonD4, &QRadioButton::toggled, this, updateDirectionConfig);
	connect(ui.radioButtonD8, &QRadioButton::toggled, this, updateDirectionConfig);

	//5.其他可选属性
	ui.checkPostProcess->setChecked(m_config->m_postOn);
	connect(ui.checkPostProcess, &QCheckBox::checkStateChanged, this, [this]() {		//51 启用后处理
		m_config->m_postOn = ui.checkPostProcess->checkState() == Qt::Checked;
		paintChanged(0);
		});
	ui.PostMode->setCurrentIndex(m_config->m_postMode);									// 后处理模式选择
	connect(ui.PostMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
		m_config->m_postMode = index;
		paintChanged(0);
		});
	ui.showGridType->setCurrentIndex(m_config->m_gridType);								// 网格显示类型选择
	connect(ui.showGridType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
		m_config->m_gridType = index;
		paintChanged(0);
		});
	ui.pushRunMode->setCurrentIndex(m_config->m_pushRunMode);								// 推挤模式选择
	connect(ui.pushRunMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
		m_config->m_pushRunMode = index;
		});
	ui.gridSizeEdit->setText(QString::number(m_config->m_gridSize, 'f', 3));		// 网格大小
	connect(ui.gridSizeEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
		bool ok;
		double value = text.toDouble(&ok);
		if (ok) {
			m_config->m_gridSize = value;
			paintChanged(0);
		}
		});
	ui.checkShowTrees->setChecked(m_config->m_showTrees);							//52 显示搜索树
	connect(ui.checkShowTrees, &QCheckBox::checkStateChanged, this, [this]() {
		m_config->m_showTrees = ui.checkShowTrees->checkState() == Qt::Checked;
		paintChanged(0);
		});
	connect(ui.treeIndex, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {	//搜索树的索引选择
		//显示的树的索引选择(初始值在updatePanelUI中设置)
		m_config->m_showTreeIndex = ui.treeIndex->itemData(index).toInt();
		paintChanged(0);
		});
	ui.flexibleOpt->setText(m_config->m_flexibleOpt);			// 输入的灵活参数数据
	connect(ui.flexibleOpt, &QLineEdit::textChanged, this, [this]() {	//editingFinished
		m_config->m_flexibleOpt = ui.flexibleOpt->text();
		});
	ui.checkDebugBreak->setChecked(m_config->m_debugFuncOn);
	connect(ui.checkDebugBreak, &QCheckBox::checkStateChanged, this, [this]() {		//53 启用中断函数
		m_config->m_debugFuncOn = ui.checkDebugBreak->checkState() == Qt::Checked;
		});
	ui.doubleInput->setText(
		QString::number(m_config->m_doubleNum0, 'f', 2) + " "
		+ QString::number(m_config->m_doubleNum1, 'f', 2) + " "
		+ QString::number(m_config->m_intNum2) + " "
		+ QString::number(m_config->m_intNum3)
	);
	connect(ui.doubleInput, &QLineEdit::editingFinished, this, [this]() {
		// 输入 3 个数字，代表坐标等数据
		QString text = ui.doubleInput->text().trimmed();
		QString normalizedText = text;

		// 统一分隔符
		normalizedText.replace(QRegularExpression("\\s+"), " ");
		normalizedText.replace(QChar(0xFF0C), QChar(' ')); // 中文逗号
		normalizedText.replace(',', ' ');
		normalizedText.replace('/', ' ');

		QString part0 = normalizedText.section(' ', 0, 0);
		QString part1 = normalizedText.section(' ', 1, 1);
		QString part2 = normalizedText.section(' ', 2, 2);
		QString part3 = normalizedText.section(' ', 3, 3);

		bool ok0 = false, ok1 = false, ok2 = false, ok3 = false;
		double v0 = part0.toDouble(&ok0);
		double v1 = part1.toDouble(&ok1);
		int    v2 = part2.toInt(&ok2);
		int    v3 = part3.toInt(&ok3);

		m_config->m_doubleNum0 = ok0 ? v0 : 0.0;
		m_config->m_doubleNum1 = ok1 ? v1 : 0.0;
		m_config->m_intNum2 = ok2 ? v2 : 0;
		m_config->m_intNum3 = ok3 ? v3 : 0;

		QString newText =
			QString::number(m_config->m_doubleNum0, 'f', 2) + " "
			+ QString::number(m_config->m_doubleNum1, 'f', 2) + " "
			+ QString::number(m_config->m_intNum2) + " "
			+ QString::number(m_config->m_intNum3);

		ui.doubleInput->setText(newText);
		});

}
