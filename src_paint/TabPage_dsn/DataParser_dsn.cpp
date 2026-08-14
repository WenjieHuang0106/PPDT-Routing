#include "DataParser_dsn.h"
#include <QRegularExpression>

using namespace std;

// 实现虚函数
bool DataParser_dsn::readFile(const QString& qFullName) {
	m_layerNameToId.clear();
	vector<QString> lines;
	QFile file(qFullName);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return false;
	}
	QTextStream in(&file);
	QString line;
	int type = -1;
	while (!in.atEnd()) {
		line = in.readLine().trimmed();		//移除行首尾空白字符，如空格，指标，换行等
		if (line.isEmpty() || line.startsWith("#") || line.startsWith("//"))
			continue;	// 跳过空行和注释行
		// 根据关键词，确定当前处于哪个数据模块
		if (line.startsWith("(PCB")) {
			type = 1;
			lines.clear();
		}
		else if (line.startsWith("(parser")) {
			parseDSNData(lines, type);
			type = 1;
			lines.clear();
		}
		else if (line.startsWith("(resolution")) {
			parseDSNData(lines, type);
			type = 2;
			lines.clear();
		}
		else if (line.startsWith("(structure")) {
			parseDSNData(lines, type);
			type = 3;
			lines.clear();
		}
		else if (line.startsWith("(placement")) {
			parseDSNData(lines, type);
			type = 4;
			lines.clear();
		}
		else if (line.startsWith("(library")) {
			parseDSNData(lines, type);
			type = 5;
			lines.clear();
		}
		else if (line.startsWith("(network")) {
			parseDSNData(lines, type);
			type = 6;
			lines.clear();
		}
		else if (line.startsWith("(wiring")) {
			parseDSNData(lines, type);
			type = 7;
			lines.clear();
		}
		lines.emplace_back(line);
	}
	parseDSNData(lines, type);
	file.close();

	// 计算边界范围，用于后面进行全屏缩放
	setMinMax();
	m_data->transform();
	m_data->setPaintData();
	return true;

}

bool DataParser_dsn::saveFile(const QString& qFullName) {

	return true;
}
bool DataParser_dsn::saveFileAs(const QString& qFullName) {
	return true;
}
void DataParser_dsn::setMinMax() {
	double x = m_data->m_dsnBoundary[1];
	double y = m_data->m_dsnBoundary[2];
	m_data->minPt = QPointF(x, y);
	m_data->maxPt = QPointF(x, y);
	size_t len_3 = m_data->m_dsnBoundary.size() - 1;
	for (size_t i = 3; i < len_3; i += 2) {
		x = m_data->m_dsnBoundary[i];
		y = m_data->m_dsnBoundary[i + 1];
		if (x < m_data->minPt.x())
			m_data->minPt.setX(x);
		if (x > m_data->maxPt.x())
			m_data->maxPt.setX(x);
		if (y < m_data->minPt.y())
			m_data->minPt.setY(y);
		if (y > m_data->maxPt.y())
			m_data->maxPt.setY(y);
	}
}

// DSN解析辅助函数实现
void DataParser_dsn::parseDSNData(vector<QString>& lines, int type) {
	switch (type) {
	case -1: break;
	case 1: parseDSNParser(lines); break;
	case 2: parseDSNResolution(lines); break;
	case 3: parseDSNStructure(lines); break;
	case 4: parseDSNPlacement(lines); break;
	case 5: parseDSNLibrary(lines); break;
	case 6: parseDSNNetwork(lines); break;
	case 7: parseDSNWiring(lines); break;
	default: break;
	}
}
void DataParser_dsn::parseDSNParser(vector<QString>& lines) {
	QRegularExpression re(R"(^\((\w+)\s*\"([^\"]+)\"\)?$)");  // 直接包含括号
	QRegularExpressionMatch match;
	for (const QString& line : lines) {
		match = re.match(line);
		if (match.hasMatch())
			m_data->m_dsnPCBInfo[match.captured(1)] = match.captured(2);
	}
}

// 解析分辨率信息
void DataParser_dsn::parseDSNResolution(vector<QString>& lines) {
	// 匹配格式："(数字)" 或 "数字"
	QRegularExpression re(R"(^\(resolution\s+mil\s+(\d+\.?\d*)\s*\)?$)");
	QRegularExpressionMatch match;
	for (const QString& line : lines) {
		match = re.match(line);
		if (match.hasMatch()) {
			bool ok = false;
			double value = match.captured(1).toDouble(&ok);
			if (ok) {
				m_data->m_dsnResolution = value;
				break;  // 如果只需要第一个匹配的数字，可以提前退出
			}
		}
	}
}
void DataParser_dsn::parseDSNStructure(vector<QString>& lines) {
	size_t lineCount = lines.size();
	for (int i = 1; i < lineCount; ++i) {
		QString& line = lines[i];
		if (line.startsWith("(boundary"))
			parse_struc_boundary(lines, i);
		else if (line.startsWith("(via"))
			parse_struc_via(lines, i);
		else if (line.startsWith("(grid"))
			parse_struc_grid(lines, i);
		else if (line.startsWith("(rule("))
			parse_struc_rule(lines, i);
		else if (line.startsWith("(layer"))
			parse_struc_layer(lines, i);
	}
}
void DataParser_dsn::parseDSNPlacement(vector<QString>& lines) {
	size_t lineCount = lines.size();
	for (int i = 1; i < lineCount; ++i) {
		QString& line = lines[i];
		if (line.startsWith("(component")) {
			//0.待读取的信息
			QString componentName = "u1";
			//1.读取内容，作为key
			QRegularExpression re(R"(^\(component\s+(\w+))");
			QRegularExpressionMatch match = re.match(line);
			if (match.hasMatch()) {
				componentName = match.captured(1);
				m_data->m_dsnComponent[componentName] = Data_dsn::DSNComponent();
			}
			//2.读取位置，并赋值
			QString& line = lines[++i];
			if (line.startsWith("(place")) {
				re.setPattern(R"(^\(place\s+\b(\w+)\b\s+([-+]?\d*\.?\d+)\s+([-+]?\d*\.?\d+)\s+\b(\w+)\b\s+([-+]?\d*\.?\d+)\)?$)");
				match = re.match(line);
				if (match.hasMatch()) {
					componentName = match.captured(1);
					m_data->m_dsnComponent[componentName].origin = QPointF(match.captured(2).toDouble(), match.captured(3).toDouble());
					m_data->m_dsnComponent[componentName].direction = match.captured(4);
					m_data->m_dsnComponent[componentName].rotation = match.captured(5).toDouble();
				}
			}
			else if (line.startsWith("(component")) {
				--i;
			}
		}
	}
}
void DataParser_dsn::parseDSNLibrary(vector<QString>& lines) {
	size_t lineCount = lines.size();
	for (int i = 1; i < lineCount; ++i) {
		QString& line = lines[i];
		if (line.startsWith("(image"))
			parse_lib_pin(lines, i);
		else if (line.startsWith("(padstack"))
			parse_lib_pad(lines, i);
	}
}
void DataParser_dsn::parseDSNNetwork(vector<QString>& lines) {
	size_t lineCount = lines.size();
	for (int i = 1; i < lineCount; ++i) {
		QString& line = lines[i];
		if (line.startsWith("(net"))
			parse_network_net(lines, i);
		else if (line.startsWith("(class"))
			parse_network_class(lines, i);
	}
}
void DataParser_dsn::parseDSNWiring(vector<QString>& lines) {
	size_t lineCount = lines.size();
	for (int i = 1; i < lineCount; ++i) {
		QString& line = lines[i];
		if (line.startsWith("(wire"))
			parse_wiring_wire(lines, i);
		else if (line.startsWith("(via"))
			parse_wiring_via(lines, i);
	}
}
void DataParser_dsn::parse_struc_boundary(vector<QString>& lines, int& i) {
	QString line = lines[i];
	vector<double> nums;
	extractAllNumbers(nums, line);
	for (double v : nums) {
		m_data->m_dsnBoundary.push_back(v);
	}
}
void DataParser_dsn::parse_struc_via(vector<QString>& lines, int& i) {
	QString line = lines[i];
	QRegularExpression re(R"(^\(via\s+(\b\w+\b(?:\s+\b\w+\b)*)\)?$)");
	QRegularExpressionMatch match = re.match(line);
	if (match.hasMatch()) {
		QString allWords = match.captured(1);
		QRegularExpression wordRe(R"(\b\w+\b)");
		QRegularExpressionMatchIterator it = wordRe.globalMatch(allWords);
		while (it.hasNext()) {
			QString word = it.next().captured(0);
			m_data->m_dsnVias.insert(word);
		}
	}
}
void DataParser_dsn::parse_struc_grid(vector<QString>& lines, int& i) {
	QString line = lines[i];
	QRegularExpression re(R"(^\(grid\s+(\w+)\s+(\d+\.?\d*)\)?$)");
	QRegularExpressionMatch match = re.match(line);
	if (match.hasMatch()) {
		m_data->m_dsnGride[match.captured(1)] = match.captured(2).toDouble();
	}

}
void DataParser_dsn::parse_struc_rule(vector<QString>& lines, int& i) {
	QString line = lines[i];
	QRegularExpression re(R"(^\(rule\(\s*(\w+)\s+(\d+\.?\d*)\s*(?:\(type\s+(\w+)\))?\)*)");
	QRegularExpressionMatch match = re.match(line);
	if (match.hasMatch()) {
		QString rule_name = match.captured(1);
		if (rule_name == "clear") {
			QString clear_type = "default";
			if (!match.captured(3).isEmpty())
				clear_type = match.captured(3);
			m_data->m_dsnRule[clear_type] = match.captured(2).toDouble();
		}
		else if (rule_name == "width") {
			m_data->m_ruleWidth = match.captured(2).toDouble();
		}
	}
}
void DataParser_dsn::parse_struc_layer(vector<QString>& lines, int& i)
{
	// 0. 待读取的信息
	QString layerName;

	// 1. 读取 layer 名称（支持数字 / 字符串）
	int bracketCount = 0;
	QRegularExpression re(R"(^\(layer\s+([^\s\)]+))");
	QRegularExpressionMatch match = re.match(lines[i]);
	if (match.hasMatch()) {
		layerName = match.captured(1);
		bracketCount = 1;
	}
	else {
		return; // 非法 layer 行，直接退出
	}

	// 2. 读取 layer 内部信息
	while (bracketCount > 0) {
		QString& line = lines[++i];
		if (line.startsWith("(type")) {
			bracketCount++;
			if (line.endsWith("))"))
				bracketCount -= 2;
			else if (line.endsWith(")"))
				bracketCount--;
			QRegularExpression typeRe(R"(^\(type\s+([^\s\)]+))");
			QRegularExpressionMatch typeMatch = typeRe.match(line);
			if (typeMatch.hasMatch()) {
				//增加映射
				int layerId;
				if (!m_layerNameToId.contains(layerName)) {
					layerId = (int)m_layerNameToId.size() + 1;
					m_layerNameToId[layerName] = layerId;
					m_data->m_dsnLayers[layerId] = typeMatch.captured(1);
				}
				else {
					layerId = m_layerNameToId[layerName];
					// 如果已经存在，可以选择是否覆盖 type
					m_data->m_dsnLayers[layerId] = typeMatch.captured(1);
				}
			}
		}
		else if (line.startsWith(")")) {
			bracketCount--;
		}
		else if (line.startsWith("(layer")) {
			// 下一个 layer，回退一行交给外层处理
			bracketCount = 1;
			--i;
			break;
		}
	}
}
/*
void DataParser_dsn::parse_struc_layer(vector<QString>& lines, int& i) {
	//0.待读取的信息
	int layer_num = 0;
	//1.读取层号，作为key
	int bracketCount = 0;
	QRegularExpression re(R"(^\(layer\s+(\d+))");
	QRegularExpressionMatch match = re.match(lines[i]);
	if (match.hasMatch()) {
		layer_num = match.captured(1).toInt();
		bracketCount = 1;
	}
	//2.读取层类型，并赋值
	while (bracketCount > 0) {
		QString& line = lines[++i];
		if (line.startsWith("(type")) {
			bracketCount++;
			if (line.endsWith("))"))
				bracketCount -= 2;
			else if (line.endsWith(")"))
				bracketCount--;
			re.setPattern(R"(^\(type\s+(\w+))");
			match = re.match(line);
			if (match.hasMatch()) {
				m_data->m_dsnLayers[layer_num] = match.captured(1);
			}
		}
		else if (line.startsWith(")"))
			bracketCount--;
		else if (line.startsWith("(layer")) {
			bracketCount = 1;
			--i;
			break;
		}
	}
}
/**/
void DataParser_dsn::parse_lib_pin(vector<QString>& lines, int& i) {
	int bracketCount = 0;
	QString componentName = "u1";
	QRegularExpression re(R"(^\(image\s+(\w+))");
	QRegularExpressionMatch match = re.match(lines[i]);
	if (match.hasMatch()) {
		componentName = match.captured(1);
		bracketCount = 1;
	}
	//读取 image 内部内容，直到括号匹配结束
	re.setPattern(R"(^\(pin\s+\b(\w+)\b\s+\b(\w+)\b\s+([-+]?\d*\.?\d+)\s+([-+]?\d*\.?\d+)\)?$)");
	while (bracketCount > 0) {
		QString& line = lines[++i];
		if (line.startsWith("(pin")) {
			bracketCount++;
			if (line.endsWith("))")) {
				bracketCount -= 2;
			}
			else if (line.endsWith(")")) {
				bracketCount--;
			}
			match = re.match(line);
			if (match.hasMatch()) {
				QString PdName = match.captured(1);
				QString PinName = match.captured(2);
				m_data->m_dsnPads[PdName] = Data_dsn::DSNPad();
				m_data->m_dsnPads[PdName].PinsName.insert(PinName);
				m_data->m_dsnPinPads[PinName] = PdName;
				m_data->m_dsnPins[PinName] = QPointF(match.captured(3).toDouble(), match.captured(4).toDouble());
			}
		}
		else if (line.startsWith(")")) {
			bracketCount--;
		}
	}
}


void DataParser_dsn::parse_lib_pad(vector<QString>& lines, int& i) {
	int bracketCount = 0;
	QString padName;

	QRegularExpression padRe(R"(^\(padstack\s+([^\s\)]+))");
	QRegularExpressionMatch match = padRe.match(lines[i]);
	if (match.hasMatch()) {
		padName = match.captured(1);
		bracketCount = 1;
	}
	else {
		return;
	}
	Data_dsn::DSNPad& pad = m_data->m_dsnPads[padName];

	// shape(type layer other...)
	QRegularExpression shapeRe(
		R"(^\(shape\(\s*(\w+)\s+([^\s\)]+)\s*(.*)\)\)\s*$)");
	QRegularExpression numRe(
		R"([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)");

	size_t lineSize = lines.size() - 1;

	while (bracketCount > 0 && i < lineSize) {
		QString line = lines[++i].trimmed();

		if (line.startsWith("(shape(")) {
			updateBracketCount(line, bracketCount);

			match = shapeRe.match(line);
			if (!match.hasMatch())
				continue;

			Data_dsn::DSNShape shape;
			shape.shapeType = match.captured(1);
			QString layerName = match.captured(2);
			if (!m_layerNameToId.contains(layerName)) {
				qDebug() << "Invalid layer name: " << layerName;
				continue;
			}
			shape.layer = m_layerNameToId[layerName];

			QString numberPart = match.captured(3);

			vector<double> values;
			auto it = numRe.globalMatch(numberPart);
			while (it.hasNext()) {
				bool ok;
				double v = it.next().captured(0).toDouble(&ok);
				if (ok) values.emplace_back(v);
			}

			if (shape.shapeType == "polygon") {
				if (values.size() >= 1) {
					shape.rotation = values[0];
					for (size_t k = 1; k + 1 < values.size(); k += 2) {
						shape.Pts.emplace_back(values[k], values[k + 1]);
					}
				}
			}
			else if (shape.shapeType == "circle") {
				if (values.size() >= 3) {
					shape.circle = CircleUI(
						values[1], values[2], values[0] / 2.0);
				}
				else if (values.size() >= 1) {
					shape.circle = CircleUI(values[0] / 2.0);
				}
			}

			pad.shapes.emplace_back(shape);
		}
		else if (line.startsWith(")")) {
			bracketCount--;
		}
		else {
			updateBracketCount(line, bracketCount);
		}
	}
}
/*
void DataParser_dsn::parse_lib_pad(vector<QString>& lines, int& i) {
	int bracketCount = 0;
	QString padName = "0";
	QRegularExpression padRe(R"(^\(padstack\s+(\w+))");
	QRegularExpressionMatch match = padRe.match(lines[i]);
	if (match.hasMatch()) {
		padName = match.captured(1);
		bracketCount = 1;
	}
	else return;

	Data_dsn::DSNPad& pad = m_data->m_dsnPads[padName];
	QRegularExpression shapeRe(R"(\(shape\(\s*(\w+)\s+((?:[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?\s*)+)\)\)\s*$)");
	QRegularExpression numRe(R"([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)");  // 匹配科学计数法数字

	// 逐行解析，直到括号匹配结束
	size_t lineSize = lines.size() - 1;
	while (bracketCount > 0 && i < lineSize) {
		QString line = lines[++i].trimmed();
		if (line.startsWith("(shape(")) {
			updateBracketCount(line, bracketCount);
			match = shapeRe.match(line);
			if (match.hasMatch()) {
				Data_dsn::DSNShape shape;
				shape.shapeType = match.captured(1);
				QString numbers = match.captured(2);

				QRegularExpressionMatchIterator it = numRe.globalMatch(numbers);
				vector<double> values;
				while (it.hasNext()) {
					bool ok;
					double num = it.next().captured(0).toDouble(&ok);
					if (ok) values.emplace_back(num);
				}
				if (values.size() >= 2) {
					shape.layer = static_cast<int>(values[0]);
					if (shape.shapeType == "polygon") {
						shape.rotation = values[1];
						for (int k = 2; k + 1 < values.size(); k += 2) {
							shape.Pts.emplace_back(values[k], values[k + 1]);
						}
					}
					else if (shape.shapeType == "circle") {
						if (values.size() >= 4) {
							shape.circle = CircleUI(values[2], values[3], values[1] / 2);	// 化为半径
						}
						else {
							shape.circle = CircleUI(values[1] / 2);	// 化为半径
						}
					}
				}
				pad.shapes.emplace_back(shape);
			}
		}
		else if (line.startsWith(")")) {
			bracketCount--;
		}
		else
			updateBracketCount(line, bracketCount);
	}
}
/**/
void DataParser_dsn::parse_network_net(vector<QString>& lines, int& i) {
	int bracketCount = 0;
	QString netName = "0";
	QRegularExpression netRe(R"(^\(net\s+(\S+))");
	QRegularExpressionMatch match = netRe.match(lines[i]);
	if (match.hasMatch()) {
		netName = match.captured(1);
		m_data->m_dsnNets[netName] = Data_dsn::DSNNet();
		updateBracketCount(lines[i], bracketCount);
	}
	else return;

	Data_dsn::DSNNet& newNet = m_data->m_dsnNets[netName];
	//QRegularExpression pinsRe(R"(\b\w+-([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\b)");  // 匹配单个 pin 值
	QRegularExpression pinsRe(R"(\b\w+-(\w+)\b)");  // 匹配单个 pin 值
	// 逐行解析，直到括号匹配结束
	size_t lineSize_1 = lines.size() - 1;
	while (bracketCount > 0 && i < lineSize_1) {
		QString line = lines[++i];
		if (line.startsWith("(pins")) {
			bracketCount++;
			if (line.endsWith("))"))
				bracketCount -= 2;
			else if (line.endsWith(")"))
				bracketCount--;
			QRegularExpressionMatchIterator it = pinsRe.globalMatch(line);
			while (it.hasNext()) {
				QRegularExpressionMatch pinMatch = it.next();
				newNet.PinsNames.emplace_back(pinMatch.captured(1));  // captured(1) 是值（如 "1e8"）
			}
		}
		else if (line.startsWith(")")) {
			bracketCount--;
		}
	}
}
void DataParser_dsn::parse_network_class(vector<QString>& lines, int& i) {
	int bracketCount = 0;
	QString netName = "0";
	QRegularExpression re(R"(\(class\s+([^)\s']+))");
	QRegularExpressionMatch match = re.match(lines[i]);
	if (match.hasMatch()) {
		netName = match.captured(1);
		bracketCount = 1;
	}
	else
		return;
	Data_dsn::DSNNet& readNet = m_data->m_dsnNets[netName];
	QRegularExpression pinsRe(R"(\b\w+-([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\b)");  // 匹配单个 pin 值
	// 逐行解析，直到括号匹配结束
	size_t lineSize_1 = lines.size() - 1;
	do {
		QString line = lines[++i];
		if (line.startsWith("(circuit"))
			parse_network_class_circuit(lines, netName, i, bracketCount);
		else if (line.startsWith("(rule")) {
			parse_network_class_rule(lines, netName, i, bracketCount);
		}
		else {
			updateBracketCount(line, bracketCount);
		}
	} while (bracketCount > 0 && i < lineSize_1);
}
void DataParser_dsn::parse_network_class_circuit(const vector<QString>& lines, const QString& shapeName, int& i, int& bracketCount) {
	i--;
	QRegularExpression re(R"(\((?:\w+)?use_via\s+(\w+)\))");
	//(use_via via0)			--bracketCount=2
	//(circuit(use_via via0)	--bracketCount=2
	//(circuit(use_via via0))	--bracketCount=1
	size_t lineSize_1 = lines.size() - 1;
	do {
		QString line = lines[++i];
		updateBracketCount(line, bracketCount);
		QRegularExpressionMatch match = re.match(line);
		if (match.hasMatch()) {
			m_data->m_dsnNets[shapeName].via = match.captured(1);
		}
	} while (bracketCount > 1 && i < lineSize_1);
}
void DataParser_dsn::parse_network_class_rule(const vector<QString>& lines, const QString& shapeName, int& i, int& bracketCount) {
	i--;
	QRegularExpression re(R"(^\((\w+)\s+([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\)?$)");
	//(width 1)
	//(clearance 0.4)
	size_t lineSize_1 = lines.size() - 1;
	do {
		QString line = lines[++i];
		updateBracketCount(line, bracketCount);		//2
		QRegularExpressionMatch match = re.match(line);
		if (match.hasMatch()) {
			QString ruleName = match.captured(1);
			if (ruleName == "width")
				m_data->m_dsnNets[shapeName].width = match.captured(2).toDouble();
			else if (ruleName == "clearance")
				m_data->m_dsnNets[shapeName].clearance = match.captured(2).toDouble();
		}
	} while (bracketCount > 1 && i < lineSize_1);
}
void DataParser_dsn::parse_wiring_wire(vector<QString>& lines, int& i) {
	int bracketCount = 0;
	Data_dsn::DSNWring wire;
	wire.wiringType = "wire";
	QString line = lines[i];

	QRegularExpression pathRe(R"(\(path\s+(\d+)\s+([-+]?\d*\.?\d+)\s+([-+]?\d*\.?\d+)\s+([-+]?\d*\.?\d+)\s+([-+]?\d*\.?\d+)\s+([-+]?\d*\.?\d+)\s*\))");
	QRegularExpression netRe(R"(\(net\s+(\S+)\)$)");
	QRegularExpression typeRe(R"(\(type\s+(\S+)\)$)");

	size_t lineSize = lines.size();
	do {
		line = lines[i];
		updateBracketCount(line, bracketCount);
		if (line.startsWith("(wire")) {
			if (line.contains("(path")) {  // case1: (wire(path ...)
				QRegularExpressionMatch match = pathRe.match(line);
				if (match.hasMatch()) {
					wire.layer = match.captured(1).toInt();
					wire.line.width = match.captured(2).toDouble();
					wire.line.setP1(QPointF(match.captured(3).toDouble(), match.captured(4).toDouble()));
					wire.line.setP2(QPointF(match.captured(5).toDouble(), match.captured(6).toDouble()));
				}
			}
			else {  // case2: (wire 和 (path 分行
				i++;
				continue;
			}
		}
		else if (line.startsWith("(path")) {  // case2 的 (path 行
			QRegularExpressionMatch match = pathRe.match(line);
			if (match.hasMatch()) {
				wire.layer = match.captured(1).toInt();
				wire.line.width = match.captured(2).toDouble();
				wire.line.setP1(QPointF(match.captured(3).toDouble(), match.captured(4).toDouble()));
				wire.line.setP2(QPointF(match.captured(5).toDouble(), match.captured(6).toDouble()));
			}
		}
		else if (line.startsWith("(net")) {
			QRegularExpressionMatch match = netRe.match(line);
			if (match.hasMatch()) {
				wire.netName = match.captured(1);
			}
		}
		else if (line.startsWith("(type")) {
			QRegularExpressionMatch match = typeRe.match(line);
			if (match.hasMatch()) {
				wire.type = match.captured(1);
			}
		}

		i++;
	} while (bracketCount > 0 && i < lineSize);
	i--;

	if (!wire.netName.isEmpty()) {
		m_data->m_dsnWirings.emplace_back(wire);
	}
}
void DataParser_dsn::parse_wiring_via(vector<QString>& lines, int& i) {
	Data_dsn::DSNWring via;
	via.wiringType = "via";  // 固定为 "via"
	int bracketCount = 0;
	QString line = lines[i].trimmed();

	QRegularExpression viaRe(R"(\(via\s+(\w+)\s+([-+]?\d*\.?\d+)\s+([-+]?\d*\.?\d+))");
	QRegularExpression netRe(R"(\(net\s+(\w+))");
	QRegularExpression typeRe(R"(\(type\s+(\w+))");

	size_t lineSize = lines.size();
	do {
		line = lines[i].trimmed();
		updateBracketCount(line, bracketCount);

		if (line.startsWith("(via")) {
			QRegularExpressionMatch match = viaRe.match(line);
			if (match.hasMatch()) {
				via.viaName = match.captured(1);  // 实际应为坐标，需调整！
				double x = match.captured(2).toDouble();
				double y = match.captured(3).toDouble();
				via.line.setP1(QPointF(x, y));    // 存储坐标到 line.p1
			}
		}
		else if (line.startsWith("(net")) {
			QRegularExpressionMatch match = netRe.match(line);
			if (match.hasMatch()) {
				via.netName = match.captured(1);
			}
		}
		else if (line.startsWith("(type")) {
			QRegularExpressionMatch match = typeRe.match(line);
			if (match.hasMatch()) {
				via.type = match.captured(1);
			}
		}

		i++;
	} while (bracketCount > 0 && i < lineSize);
	i--;
	if (!via.netName.isEmpty()) {
		m_data->m_dsnWirings.emplace_back(via);
	}
}

void DataParser_dsn::updateBracketCount(const QString& line, int& bracketCount) {
	for (QChar ch : line) {
		if (ch == '(') ++bracketCount;
		else if (ch == ')') --bracketCount;
	}
}

// 从字符串中提取所有数字
void DataParser_dsn::extractAllNumbers(vector<double>& out, const QString& s) {
	QRegularExpression re("[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?");
	QRegularExpressionMatchIterator it = re.globalMatch(s);
	while (it.hasNext()) {
		QRegularExpressionMatch m = it.next();
		out.push_back(m.captured(0).toDouble());
	}
}