#include "TabPage_dsn.h"
#include <QVBoxLayout>
#include <QMessageBox>

using namespace std;

void TabPage_dsn::fillData(AppConfig* conf) {
	m_config = conf;
	// 2.将文件数据写入数据对象
	m_dataAcquired = m_parser->readFile(m_qFullName);
	// NOTE: even if readFile returns true, a defensive nullptr check is
	// still required because in some CWD-error scenarios (e.g. VS debugger
	// working dir = build/ so configs can't be loaded) the parser returns
	// true but leaves m_data == nullptr. Dereferencing m_data below (in
	// onConfigChanged, routingRunBegin, setData2) would then cause an
	// immediate segfault which manifests as "program crashes when opening a
	// file" especially in Debug builds (Release masks many null-deref UBs).
	if (!m_dataAcquired || !m_data) {
		QMessageBox::warning(this, "提示",
			QString("文件读取失败：%1").arg(m_qFullName));
		return;
	}
	m_runFinished = false;
	// 3.创建绘图窗口Diagram，
	std::shared_ptr<DiagramDataBase> diagramData = std::static_pointer_cast<DiagramDataBase>(
		std::shared_ptr<Data_dsn>(m_data, [](auto*) {})); // 共享指针但不删除
	m_diagram = new Diagram(diagramData, m_config, this);
	m_algm = new RoutingController(m_data, m_config);
	// 4.计算并绘制飞线
	m_algm->routingRunBegin();
	m_diagram->setData2();
	onConfigChanged();		// 更新显示/隐藏等选项信息

	// 5.设置布局
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_diagram);
	setLayout(layout);
}
void TabPage_dsn::refreshUI() {
	m_diagram->refresh();
}

//槽函数
void TabPage_dsn::pressRunButton() {
	// Guard against running before fillData succeeded (prevents nullptr crash
	// if user somehow triggers this action on a partially initialized tab).
	if (!m_algm || !m_diagram || !m_data) return;
	m_algm->routingRun(m_qFileName);
	m_diagram->setData2();
	m_diagram->setData3();
	emit requestUpdatePanelUI((int)m_data->m_treesLines.size());
	refreshUI();
}
void TabPage_dsn::onConfigChanged(int type) {
	if (!m_data || !m_config || !m_diagram) return;
	if (type == 0) {
		const Style_dsn& sInfo = m_data->m_styleInfo;
		//1.飞线显隐性更新
		m_data->m_styles.getLineStyle(sInfo.flyLineKey)->setVisible(m_config->m_showFlyLines);
		//2.路径线显隐性更新
		for (const QString& key : sInfo.pathLineKeys) {
			m_data->m_styles.getLineStyle(key)->setVisible(m_config->m_showPahts);
		}
		//3.via显隐性更新,与路径一致
		bool show_vias = m_config->m_showPahts && m_config->m_show_vias;
		m_data->m_styles.getPolygonStyle(sInfo.viaCicleKey)->setVisible(show_vias);

		//4.焊盘显隐性更新
		const QString& obs1key = sInfo.getPadPolyKey(0);
		m_data->m_styles.getPolygonStyle(obs1key)->setVisible(m_config->m_showObs1);
		const QString& obs2key = sInfo.getPadPolyKey(1);
		m_data->m_styles.getPolygonStyle(obs2key)->setVisible(m_config->m_showObs2);
		//5.规划点显隐性更新
		m_data->m_styles.getPointStyle(sInfo.planningPtKey)->setVisible(m_config->m_showPPs);
		//6.树线显隐性更新
		m_diagram->setShowTreeIndex(m_config->m_showTreeIndex);
		refreshUI();
	}
	else if (type == 2) {
		if (m_algm) {
			m_algm->setNetMST();
			refreshUI();
		}
	}
}
void TabPage_dsn::tranformChanged() {
	if (m_diagram) {
		m_diagram->initViewTransform();
		refreshUI();
	}
}
void TabPage_dsn::leftPressRun(SelectedTarget* target) {
	if (!m_algm || !target) return;
	if (target->lineSelected) {
		m_algm->pushLineRunBegin(target->sLine, *target->linesToRun);
	}
	else if (target->pinSelected) {
		m_algm->pushPinRunBegin(target->nearistPt, *target->linesToRun);
	}
}
void TabPage_dsn::leftPressMoveRun(SelectedTarget* target) {
	if (!m_algm || !target || !m_config) return;
	if (target->lineSelected) {
		m_algm->pushLineRun(target->offset, false, false);
	}
	else if (target->pinSelected) {
		m_algm->pushPinRun(target->offset, false);
	}
}
void TabPage_dsn::leftReleaseRun(SelectedTarget* target) {
	if (!m_algm || !target || !m_config || !m_diagram) return;
	if (target->lineSelected) {
		m_algm->pushLineRun(target->offset, m_config->m_onGrids, true);
	}
	else if (target->pinSelected) {
		m_algm->pushPinRun(target->offset, m_config->m_onGrids);
	}
	m_diagram->setData1();
	m_diagram->setData2();
	refreshUI();
}