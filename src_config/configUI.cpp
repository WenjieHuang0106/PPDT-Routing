#include "configUI.h"
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QString>
#include <QFileDialog>
#include <QInputDialog>

void ConfigUI::configUpdate(const QString& qFilePath, const QString& qFileName) {
	m_qFilePath = qFilePath;
	m_qFileName = qFileName;
}

bool ConfigUI::read_config() {
	QString configPath = "src_config/config.config";
	QString defaultPath = "src_config/config_default.config";
	QFile configFile(configPath);
	// 配置文件不存在，则用默认配置创建一个配置文件
	if (!configFile.exists()) {
		QFile defaultFile(defaultPath);
		if (!defaultFile.exists()) {
			QMessageBox::warning(nullptr, tr("警告："),
				tr("默认配置文件缺失：\n%1").arg(defaultPath));
			return false;
		}
		if (!defaultFile.copy(configPath)) {
			QMessageBox::warning(nullptr, tr("警告："),
				tr("无法创建配置文件：\n%1").arg(configPath));
			return false;
		}
	}
	// 正常读取配置文件
	if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QMessageBox::warning(nullptr, tr("警告："),
			tr("读取配置文件失败！"));
		return false;
	}
	QTextStream in(&configFile);
	in.setAutoDetectUnicode(true); // 自动检测UTF-BOM
	while (!in.atEnd()) {
		QString line = in.readLine().trimmed();
		if (line.isEmpty()) continue;	// 跳过空行

		if (line.startsWith("defaltPath"))
			m_qFilePath = line.section(' ', 1);
		else if (line.startsWith("defaltName"))
			m_qFileName = line.section(' ', 1);
		else if (line.startsWith("viewOpt")) {
			m_flip_left_right = line.section(' ', 1, 1).toInt();;	//01.翻转X轴(左右)
			m_flip_up_down = line.section(' ', 2, 2).toInt();;		//02.翻转Y轴(上下)
			m_rotate_90 = line.section(' ', 3, 3).toInt();;			//03.旋转90度
		}
		else if (line.startsWith("algmOpt")) {
			m_preViaAlctOn = line.section(' ', 1, 1).toInt();;		//11.预via分配
			m_GNDRouteOn = line.section(' ', 2, 2).toInt();;		//12.GND特殊布线
			m_VCCRouteOn = line.section(' ', 3, 3).toInt();;		//13.VCC特殊布线
			m_diffRouteOn = line.section(' ', 4, 4).toInt();;		//14差分布线
			m_4_8Tree = line.section(' ', 5, 5).toInt();;			//15.4/8方向搜索树
            m_autoPush = line.section(' ', 6, 6).toInt();			//16.自动推线，几何重构
			m_autoWriteExpResult = line.section(' ', 7, 7).toInt();	//17.自动写入实验结果
		}
		else if (line.startsWith("dispOpt")) {
			m_showObs1 = line.section(' ', 1, 1).toInt();;			//21.显示第1层障碍物
			m_showObs2 = line.section(' ', 2, 2).toInt();;			//22.显示第2层障碍物
			m_showFlyLines = line.section(' ', 3, 3).toInt();;		//23.显示飞线
			m_showPahts = line.section(' ', 4, 4).toInt();;			//24.显示布线结果
			m_showPPs = line.section(' ', 5, 5).toInt();;			//55.显示规划点
			m_show_vias = line.section(' ', 6, 6).toInt();;			//26.显示过孔
			m_showPinCenters = line.section(' ', 7, 7).toInt();;	//27.显示Pin中心
		}
		else if (line.startsWith("boolOpt")) {
			m_postOn = line.section(' ', 1, 1).toInt();;			//51.启用后处理
			m_showTrees = line.section(' ', 2, 2).toInt();;			//52.显示树
			m_debugFuncOn = line.section(' ', 3, 3).toInt();;		//53.启用调试中断
		}
		else if (line.startsWith("m_directionOp"))
			m_directionOp = line.section(' ', 1).toInt();
		else if (line.startsWith("m_gridType"))
			m_gridType = line.section(' ', 1).toInt();
		else if (line.startsWith("m_pushRunMode"))
			m_pushRunMode = line.section(' ', 1).toInt();
		else if (line.startsWith("m_postMode"))
			m_postMode = line.section(' ', 1).toInt();
		else if (line.startsWith("m_minimalScreenGridSize"))
			m_minimalScreenGridSize = line.section(' ', 1).toFloat();
		else if (line.startsWith("m_gridSize"))
			m_gridSize = line.section(' ', 1).toFloat();
		else if (line.startsWith("m_showTreeIndex "))
			m_showTreeIndex = line.section(' ', 1).toInt();
		else if (line.startsWith("m_flexibleOpt"))
			m_flexibleOpt = line.section(' ', 1);
		else if (line.startsWith("m_doubleNum")) {
			m_doubleNum0 = line.section(' ', 1, 1).toDouble();
			m_doubleNum1 = line.section(' ', 2, 2).toDouble();
			m_intNum2 = line.section(' ', 3, 3).toInt();
			m_intNum3 = line.section(' ', 4, 4).toInt();
		}
		else if (line.startsWith("#")) {
			QMessageBox::warning(nullptr, tr("警告："), tr("配置文件中存在未知的行！"));
			continue;
		}
	}
	configFile.close();
	return true;
}
bool ConfigUI::write_config()const {
	// 创建或覆盖配置文件
	QString filePath = "src_config/config.config";
	// 1. 确保目录存在
	QFileInfo fileInfo(filePath);
	QDir dir = fileInfo.dir();
	if (!dir.exists()) {
		if (!dir.mkpath(".")) {   // 创建当前目录
			QMessageBox::warning(nullptr, "错误", "无法创建配置目录");
			return false;
		}
	}
	// 2. 创建或覆盖配置文件
	QFile outputFile(filePath);
	if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QMessageBox::warning(nullptr, "错误", "无法创建配置文件");
		return false;
	}

	QTextStream out(&outputFile);

	// 直接写入所有配置项，确保每个配置项都存在
	out << "defaltPath " << m_qFilePath << Qt::endl;
	out << "defaltName " << m_qFileName << Qt::endl;
	out << "viewOpt "
		<< m_flip_left_right << " " //01.翻转X轴(左右)
		<< m_flip_up_down << " "    //02.翻转Y轴(上下)
		<< m_rotate_90 << " "       //03.旋转90度
		<< Qt::endl;
	out << "algmOpt "
		<< m_preViaAlctOn << " "    //11.预via分配
		<< m_GNDRouteOn << " "      //12.GND特殊布线
		<< m_VCCRouteOn << " "      //13.VCC特殊布线
		<< m_diffRouteOn << " "     //14.差分布线
		<< m_4_8Tree << " "         //15.4/8方向搜索树
        << m_autoPush << " "		//16.自动推线，几何重构
        << m_autoWriteExpResult << " "
		<< Qt::endl;
	out << "dispOpt "
		<< m_showObs1 << " "        //21.显示第1层障碍物
		<< m_showObs2 << " "        //22.显示第2层障碍物
		<< m_showFlyLines << " "    //23.显示飞线
		<< m_showPahts << " "       //24.显示布线结果
		<< m_showPPs << " "         //25.显示规划点
		<< m_show_vias << " "       //26.显示过孔
		<< m_showPinCenters << " "  //27.显示Pin中心
		<< Qt::endl;
	out << "boolOpt "
		<< m_postOn << " "          //51.启用后处理
		<< m_showTrees << " "       //52.显示树
		<< m_debugFuncOn << " "     //53.启用调试中断
		<< Qt::endl;
	out << "m_directionOp " << m_directionOp << Qt::endl;
	out << "m_gridType " << m_gridType << Qt::endl;
	out << "m_pushRunMode " << m_pushRunMode << Qt::endl;
	out << "m_postMode " << m_postMode << Qt::endl;
	out << "m_minimalScreenGridSize " << m_minimalScreenGridSize << Qt::endl;
	out << "m_gridSize " << m_gridSize << Qt::endl;
	out << "m_showTreeIndex " << m_showTreeIndex << Qt::endl;
	out << "m_flexibleOpt " << m_flexibleOpt << Qt::endl;
	out << "m_doubleNum " << m_doubleNum0 << " " << m_doubleNum1 << " " << m_intNum2 << " " << m_intNum3 << Qt::endl;

	outputFile.close();
	return true;
}