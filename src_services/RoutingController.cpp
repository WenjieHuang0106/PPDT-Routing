#include "RoutingController.h"
#include "../src_algorithms/src_dsn/MST.h"
#include <chrono>

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <sstream> 
#include <iostream>

using namespace std;

inline static bool openResultFile(const QString& rawFileName, QFile& file, QTextStream& ts)
{
	QString dirPath = "data/result";
	QDir().mkpath(dirPath);

	QFileInfo fi(rawFileName);
	QString finalName =
		QDateTime::currentDateTime().toString("yyyyMMddHHmmss") +
		"_" + fi.completeBaseName() + ".txt";

	QString fullPath = dirPath + "/" + finalName;

	file.setFileName(fullPath);

	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		qWarning() << "Failed to open file:" << fullPath;
		return false;
	}

	ts.setDevice(&file);
	ts.setGenerateByteOrderMark(true);

	return true;
}

//寻路
void RoutingController::routingRunBegin() {
	//1.数据初始化
	dataInit();
	//2.计算最小生成树MST（填充m_netTrees，m_flyLines）
	setNetMST();
	//3.输出基础信息
	int pinsSum1 = (int)m_pads.size();
	int pinsSum2 = 0;
	for (const auto& [netName, net] : m_nets) {
		pinsSum2 += net.size();
	}
	cout << "Min:(" << static_cast<int>(ceil(m_bound[0])) << ", " << static_cast<int>(ceil(m_bound[1])) << "),"
		<< "Max:(" << static_cast<int>(ceil(m_bound[2])) << ", " << static_cast<int>(ceil(m_bound[3])) << "),"
		<< ", Net:" << m_nets.size() << ", pinsSum:" << pinsSum1 << ", dsnPathLen:" << m_totalLenOfRoutedDsn << endl;
}
void RoutingController::setNetMST() {
	unordered_map<string, PinPad>* steinerNodes_ptr = &m_preVias;
	
	steinerNodes_ptr = &m_preVias;
	m_preVias.clear();
	m_solver.setSteinerNodesOn(m_config->m_preViaAlctOn);			//是否启用过孔预分配
	m_solver.computeSteinerTrees(m_nets, m_netsInfos, m_netTrees);					//计算最小生成树MST
	m_solver.insertSteinerNodes(m_netsInfos, m_viaInfos, m_netTrees, m_preVias);	//插入斯坦纳点，预过孔分配
	
	m_solver.setFlyLines(m_netTrees, m_flyLines);
	fillPaintFlyLines();

	if (m_router) delete m_router;
	m_router = new RouterMeshless(&m_netTrees, &m_pads, steinerNodes_ptr, &m_bound, &m_viaInfos, &m_netsInfos, &m_nets);
}
void RoutingController::routingRun(const QString& qFileName) {
	//1.前端传入算法的数据
	Point inputPt(m_config->m_doubleNum0, m_config->m_doubleNum1);
	string flexibleOpt = m_config->m_flexibleOpt.toLatin1().constData();

	//2.布线器参数设置
	bool saveExpResult = m_config->m_autoWriteExpResult;
	vector<bool> boolOps = {
		m_config->m_postOn,
		m_config->m_GNDRouteOn,
		m_config->m_VCCRouteOn,
		m_config->m_diffRouteOn,
		m_config->m_autoPush,
		m_config->m_4_8Tree
	};
	m_router->setDirectionOpt(m_config->m_directionOp);	//方向约束设置
	m_router->setRouterOption(boolOps);
	m_router->setDebugOpt(flexibleOpt, m_config->m_debugFuncOn, inputPt, m_config->m_intNum2);
	m_router->setPushTimes(m_config->m_intNum3);

	//3.记录实验数据
	QFile file;
	QTextStream ofsObj;
	QTextStream* ofs = nullptr;
	if (saveExpResult && openResultFile(qFileName, file, ofsObj)) {
		ofs = &ofsObj;
	}
	int runExpermentID = 0;
	switch (runExpermentID) {
	case 0: experment0(ofs); break;
	default: break;
	}

	//4.获取计算结果
	m_treesHeads = m_router->getTreesHeadsOrdered();
	m_pathsShapes = m_router->getPaths();
	m_planningPts = m_router->getPlanningPts();
	m_vias = m_router->getVias();

	//5.反馈前前端绘图
	m_solver.setFlyLines(m_netTrees, m_flyLines);
	fillPaintFlyLines();

	fillPaintPathLines();		// 绘制路径
	fillPlanningPt();
	fillPaintTrees();
	m_config->save();	// 每次执行完即更新，防止中断调试导致的属性丢失，下次打开又要重新设置
}
void RoutingController::experment0(QTextStream* ofs) {
	//run only once
	if (ofs) {
		*ofs << "Layers\tPairs\tFound\tFailed\tRateP\tAvgLen\tTNodes\tVia\tDRC\tRateN\t";
		*ofs << "Time\tgridSize\n";
	}
	//4.布线器数据清理,建立空间网格索引，生成初始规划点
	double gridSize = m_config->m_gridSize;
	auto t1 = chrono::high_resolution_clock::now();
	m_router->routerReset(gridSize);
	vector<string> routingInfo;
	m_router->run(routingInfo);
	auto t2 = chrono::high_resolution_clock::now();
	//5.输出布线时间
	auto time = chrono::duration_cast<chrono::milliseconds>(t2 - t1).count();
	cout << "Time:" << time << " ms" << ", gridSize:" << gridSize << endl;
	if (ofs) {
		for (auto& info : routingInfo) {
			*ofs << QString::fromLocal8Bit(info.c_str()) << "\t";
		}
		*ofs << time << "\t" << gridSize << "\n";
		ofs->flush();
	}
	cout << "\n\n\n\n==================================================================================================" << endl;
}


void RoutingController::dataInit() {
	m_viaInfos.clear();
	m_pads.clear();
	m_nets.clear();
	m_netsInfos.clear();
	m_netTrees.clear();
	m_flyLines.clear();
	// 1.设置边界
	QPointF* minp = m_data->getMinPoint();
	QPointF* maxp = m_data->getMaxPoint();
	m_bound = { minp->x(), minp->y(), maxp->x(), maxp->y() };
	// 2.填充m_viaInfos，从QSet<QString> m_dsnVias，即m_data->m_dsnVias中读取
	for (const QString& viaName : m_data->m_dsnVias) {
		const vector<Data_dsn::DSNShape>& shapes = m_data->m_dsnPads[viaName].shapes;
		if (shapes.empty()) continue;
		double radius = shapes[0].circle.radius;
		vector<int> layers;
		for (const Data_dsn::DSNShape& shape : shapes) {
			layers.emplace_back(shape.layer);
		}
		m_viaInfos[viaName.toLatin1().constData()] = ViaInfo(radius, layers);
	}
	// 3.填充m_pads
	double clear_default_smd = m_data->m_dsnRule["default_smd"];
	for (auto it = m_data->m_dsnPins.begin(); it != m_data->m_dsnPins.end(); ++it) {
		// 3.1设置焊盘坐标
		QString qPinName = it.key();
		string pinPadName(it.key().toLatin1().constData());
		double pinX = it->x();
		double pinY = it->y();
		m_pads.insert(make_pair(pinPadName, PinPad(Point(pinX, pinY), pinPadName, "", clear_default_smd)));
		PinPad& onePad = m_pads[pinPadName];
		// 3.2设置焊盘的pad多边形
		if (!m_data->m_dsnPins.contains(qPinName) || !m_data->m_dsnPinPads.contains(qPinName))
			continue;
		QString& padName = m_data->m_dsnPinPads[qPinName];
		if (!m_data->m_dsnPads.contains(padName))
			continue;
		const vector<Data_dsn::DSNShape>& shapes = m_data->m_dsnPads[padName].shapes;
		if (shapes.empty()) {
			qDebug() << "shapes is empty:" << padName;
			continue;
		}
		// 3.3先处理第一个shape的形状，后面都与第一个相同
		const Data_dsn::DSNShape& shape0 = shapes[0];
		vector<Line> edges;
		double radius = -1;		// 如果是多边形，则保持-1，如果是圆，则更新为半径>0
		if (shape0.shapeType == "polygon") {
			double x1 = shape0.Pts[0].x(), y1 = shape0.Pts[0].y();
			double x2 = shape0.Pts[1].x(), y2 = shape0.Pts[1].y();
			size_t pointCount = shape0.Pts.size();
			bool isClosed = (pointCount > 1 && shape0.Pts[0] == shape0.Pts[pointCount - 1]);
			size_t effectivePointCount = isClosed ? pointCount - 1 : pointCount;
			if (effectivePointCount < 2) continue;
			for (size_t i = 0; i < effectivePointCount; ++i) {
				size_t next_i = (i + 1) % effectivePointCount;
				double x1 = shape0.Pts[i].x() + pinX;
				double y1 = shape0.Pts[i].y() + pinY;
				double x2 = shape0.Pts[next_i].x() + pinX;
				double y2 = shape0.Pts[next_i].y() + pinY;
				if (x1 == x2 && y1 == y2) {
					continue;
				}
				Line line(Point(x1, y1), Point(x2, y2));
				edges.emplace_back(line);
			}
		}
		else if (shape0.shapeType == "circle") {
			double x = shape0.circle.x() + pinX;
			double y = shape0.circle.y() + pinY;
			radius = shape0.circle.radius;
		}
		// 3.4每个shape形状相同，但layer不同，用形状和layer构造一个PinPad
		if (radius > 0)
			for (const Data_dsn::DSNShape& shape : shapes) {
				onePad.addShape(shape.layer, shape.circle.radius, Point(shape.circle.x(), shape.circle.y()));
			}
		else
			for (const Data_dsn::DSNShape& shape : shapes)
				onePad.addShape(shape.layer, edges);
	}
	// 4.填充m_nets，m_net_vias
	for (auto it = m_data->m_dsnNets.begin(); it != m_data->m_dsnNets.end(); ++it) {
		string netName(it.key().toLatin1().constData());
		//if (netName == "GND") continue;
		vector<PinPad*> oneNet;
		for (const QString& shapeName : it.value().PinsNames) {
			PinPad* pad = &m_pads[shapeName.toLatin1().constData()];
			pad->setNetName(netName);
			oneNet.emplace_back(pad);
		}
		m_nets[netName] = oneNet;
		string viaName(it.value().via.toLatin1().constData());
		m_netsInfos[netName] = NetInfo(viaName, it->width, it->clearance);
	}
	// 5.如果该文件已完成布线，则统计布线长度
	m_totalLenOfRoutedDsn = 0;
	if (!m_data->m_paths.empty() && !m_data->m_paths.front().empty()) {
		for (const auto& layer_paths : m_data->m_paths) {
			for (const auto& line : layer_paths) {
				// LineUI 继承自 QLineF，可以直接使用 length() 方法
				m_totalLenOfRoutedDsn += line.length();
			}
		}
	}
}
void RoutingController::fillPaintFlyLines() {
	//1.绘制飞线
	vector<vector<LineUI>>& ui_flyLines = m_data->m_flyLines;
	ui_flyLines.clear();
	for (auto& lines : m_flyLines) {
		vector<LineUI> linesUI;
		for (auto& line : lines) {
			LineUI lui(line.Pt1.x, line.Pt1.y, line.Pt2.x, line.Pt2.y);
			linesUI.emplace_back(lui);
		}
		ui_flyLines.emplace_back(linesUI);
	}
	//2.绘制过孔
	vector<CircleUI>& ui_vias = m_data->m_viaCircles;
	ui_vias.clear();
	addPreViasToUi(ui_vias);
}
void RoutingController::fillPaintPathLines() {
	//1.绘制路径
	if (m_pathsShapes) {
		vector<vector<LineUI>>& ui_paths = m_data->m_paths;
		ui_paths.clear();
		for (const auto& [pNode, shape] : *m_pathsShapes) {
			const vector<PathLine>& pLines = shape.edges;
			ui_paths.emplace_back(vector<LineUI>());
			vector<LineUI>& onePath = ui_paths.back();
			for (const PathLine& line : pLines) {
				LineUI lui(line.p1->pos.x, line.p1->pos.y, line.p2->pos.x, line.p2->pos.y, line.layer, line.width);
				lui.layer = line.layer;
				onePath.emplace_back(lui);
			}
		}
	}
	//2.绘制过孔
	vector<CircleUI>& ui_vias = m_data->m_viaCircles;
	ui_vias.clear();
	//2.1 预分配的过孔
	addPreViasToUi(ui_vias);
	//2.2.布线产生的过孔
	if (m_vias) {
		for (const auto& [pos, pad] : *m_vias) {
			CircleUI ccPin(pos.x, pos.y, pad.r);
			ui_vias.emplace_back(ccPin);
		}
	}
}
void RoutingController::addPreViasToUi(vector<CircleUI>& ui_vias) {
	if (!m_preVias.empty()) {
		for (const auto& [pinName, pad] : m_preVias) {
			const Point& pos = pad.pos;
			CircleUI ccPin(pos.x, pos.y, pad.r);
			ui_vias.emplace_back(ccPin);
		}
	}
}
void RoutingController::fillPaintTrees() {
	if (!m_treesHeads) return;
	vector<vector<LineUI>>& ui_trees = m_data->m_treesLines;
	ui_trees.clear();
	for (const auto& tree : *m_treesHeads) {
		if (!tree) continue;
		ui_trees.emplace_back(vector<LineUI>());
		vector<LineUI>& ui_oneTree = ui_trees.back();

		queue<PathTree*> nodeQueue;
		nodeQueue.push(tree);
		while (!nodeQueue.empty()) {
			PathTree* currentNode = nodeQueue.front();
			nodeQueue.pop();
			for (PathTree* child : currentNode->children) {
				if (!child) continue;
				LineUI lui(currentNode->pos.x, currentNode->pos.y, child->pos.x, child->pos.y, currentNode->layer);	//不同层树颜色不同，省略层为灰色
				ui_oneTree.push_back(lui);
				nodeQueue.push(child);
			}
		}
	}
}
void RoutingController::fillPlanningPt() {
	if (!m_planningPts) return;
	vector<QPointF>& ui_planningPts = m_data->m_planningPts;
	ui_planningPts.clear();
	for (const auto& pt : *m_planningPts) {
		ui_planningPts.emplace_back(QPointF(pt.x, pt.y));
	}
}

void RoutingController::pushLineRunBegin(LineUI& line, vector<LineUI>& lines) {
	if (!m_pathsShapes) return;
	vector<vector<LineUI>>& ui_paths = m_data->m_paths;
	int i = 0;
	m_selectShape = nullptr;
	m_selectPathPreNode = nullptr;
	for (auto& [pNode, shape] : *m_pathsShapes) {
		if (&ui_paths[i++] != &lines) continue;
		m_selectShape = &shape;
		int j = 0;
		m_selectPathPreNode = nullptr;
		for (auto& nodeLine : shape.edges) {
			//寻找被鼠标选中的线段
			if (fabs(line.p1().x() - nodeLine.p1->pos.x) < MapMinValue
				&& fabs(line.p1().y() - nodeLine.p1->pos.y) < MapMinValue
				&& fabs(line.p2().x() - nodeLine.p2->pos.x) < MapMinValue
				&& fabs(line.p2().y() - nodeLine.p2->pos.y) < MapMinValue) {
				m_selectPathPreNode = nodeLine.p1;
				break;
			}
		}
		break;
	}
	m_ui_wires = &lines;
}
bool RoutingController::pushLineRun(const QPointF& offset, const bool inGreed, bool algUpdate) {
	if (!m_data) {
		cout << "pushLineRun: m_data is nullptr" << endl;
		return false;
	}
	
	if (!m_selectShape || !m_selectPathPreNode)return false;
	//1.定义算法的输入数据
	PolyShape* shapeTmp = m_selectShape->copy();
	PathNode* nodeInput = nullptr;
	Point offsetInput;
	bool inputGot = setPushLineData(shapeTmp, nodeInput, offsetInput, offset);
	if (!inputGot) return false;
	bool canPush = m_router->checkBeforPush(nodeInput, m_selectShape, shapeTmp, offsetInput, false);

	//2.更新前端与算法数据
	switch (m_config->m_pushRunMode) {
	case 0:		//0.Off
		pushLineUIDataUpdate(shapeTmp, algUpdate);
		break;
	case 1:		//1.Block
		if (canPush) {
			if (m_shapeSuccessed) {
				m_shapeSuccessed->deleteNodes();
				delete m_shapeSuccessed;
				m_shapeSuccessed = nullptr;
			}
			m_shapeSuccessed = shapeTmp->copy();
		}
		pushLineUIDataUpdate(m_shapeSuccessed, algUpdate);
		break;
	default:
		pushLineUIDataUpdate(shapeTmp, algUpdate);
	}
	//3.释放算法数据
	if (shapeTmp) {
		shapeTmp->deleteNodes();
		delete shapeTmp;
		shapeTmp = nullptr;
	}
	return true;
}
bool RoutingController::setPushLineData(PolyShape* shapeInput, PathNode*& nodeInput, Point& offsetInput, const QPointF& offsetUI) {
	if (m_selectShape->edges.empty()) return false;
	if (offsetUI.x() == 0 && offsetUI.y() == 0) return false;
	//1.推挤输入的线段节点
	PathNode* algHead = m_selectShape->edges.front().p1;
	if (algHead == m_selectPathPreNode)
		nodeInput = shapeInput->edges[0].p1;
	else {
		PathNode* cur = algHead->next;
		nodeInput = shapeInput->edges[0].p2;
		while (cur && cur != algHead) {
			if (cur == m_selectPathPreNode)
				break;
			cur = cur->next;
			nodeInput = nodeInput->next;
		}
	}
	offsetInput = Point(offsetUI.x(), offsetUI.y());
	return true;
}
void RoutingController::pushLineUIDataUpdate(PolyShape* shape, bool algUpdate) {
	if (!shape) return;
	//1.更新前端数据
	m_ui_wires->clear();
	const vector<PathLine>& pathInput = shape->edges;
	for (const auto& line : pathInput) {
		const Point& p1 = line.p1->pos;
		const Point& p2 = line.p2->pos;
		m_ui_wires->emplace_back(LineUI(p1.x, p1.y, p2.x, p2.y), line.layer, line.width);
	}
	//更新算法里面的数据
	if (algUpdate) {	// 鼠标释放时 algUpdate = true
		m_router->pushLineDataUpdate(shape, m_selectShape);
		//4.更新过孔
		vector<CircleUI>& ui_vias = m_data->m_viaCircles;
		ui_vias.clear();
		//预分配的过孔
		addPreViasToUi(ui_vias);
		//布线产生的过孔
		if (m_vias) {
			for (const auto& [pos, pad] : *m_vias) {
				CircleUI ccPin(pos.x, pos.y, pad.r);
				ui_vias.emplace_back(ccPin);
			}
		}
		if (m_shapeSuccessed) {
			m_shapeSuccessed->deleteNodes();
			delete m_shapeSuccessed;
			m_shapeSuccessed = nullptr;
		}
	}
}
/*
void RoutingController::pushPinRunBegin(QPointF pt, vector<LineUI>& lines) {
	m_selectPt = pt;
	m_beginLines.clear();
	if (&lines == nullptr) {
		m_ui_wires = nullptr;  // 或者设置为其他安全值
		return;
	}
	for (auto& line : lines) {
		double x1 = (double)line.x1(), y1 = (double)line.y1();
		double x2 = (double)line.x2(), y2 = (double)line.y2();
		m_beginLines.emplace_back(Line(Point(x1, y1), Point(x2, y2), line.layer, line.width));
	}
	m_ui_wires = &lines;
}
bool RoutingController::pushPinRun(const QPointF& offset, bool inGreed) {
	if (!m_data) {
		cout << "pushPinRun: m_data is nullptr" << endl;
		return false;
	}
	//1.定义算法的输入数据
	vector<Line> polyLines_input;
	Point pt;
	Point offset_input;
	//3.设置算法输入数据
	bool res = setPushPinData(polyLines_input, pt, offset_input, offset);

	//4.创建推挤对象,设置基本参数m_config->m_onGrids, m_config->m_postOn
	SlideLine slide;
	int mode = 1;
	slide.setMode(mode);
	slide.setInGreed(inGreed);

	//5.执行算法
	int result = slide.pushOnePt(polyLines_input, pt, offset_input);

	//6.反馈给前端
	if (result == 0) {	//推中间点
		pushLineDataUpdate(polyLines_input);
		return true;
	}
	else if (result == 1) {	//推Pin,清空布线信息
		pushLineDataUpdate(polyLines_input);
		m_data->m_paths.clear();
		return true;
	}
	else {
		cout << "pushLineRun: pushSingleLine failed, error code= " << result << " ###" << endl;
		return false;
	}
	return true;

}
bool RoutingController::setPushLineData(vector<Line>& polyLines_input, Line& line_input, Point& offset_input, const QPointF& offset_ui) {
	bool zeroOffset = offset_ui.x() == 0 && offset_ui.y() == 0;
	if (zeroOffset || m_beginLines.empty()) return false;	//偏移量为0 或 输入数据为空，结束算法
	if (m_selectLine.length() == 0) return false;	//选取的线段为零线段，结束算法

	polyLines_input = vector<Line>(m_beginLines);
	line_input = Line(Point(m_selectLine.x1(), m_selectLine.y1()), Point(m_selectLine.x2(), m_selectLine.y2()));
	offset_input = Point(offset_ui.x(), offset_ui.y());
	return true;
}
void RoutingController::pushLineDataUpdate(vector<Line>& polyLines_input) {
	m_ui_wires->clear();
	for (auto& line : polyLines_input) {
		m_ui_wires->emplace_back(LineUI(line.Pt1.x, line.Pt1.y, line.Pt2.x, line.Pt2.y), line.layer, line.width);
	}
}
bool RoutingController::setPushPinData(vector<Line>& polyLines_input, Point& pt_input, Point& offset_input, const QPointF& offset_ui) {
	polyLines_input = vector<Line>(m_beginLines);
	pt_input = Point(m_selectPt.x(), m_selectPt.y());
	offset_input = Point(offset_ui.x(), offset_ui.y());
	return true;
}
/**/