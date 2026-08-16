#pragma once
#include "ConfigBase.h"
#include <QPen>
#include <QColor>

enum PainState {
	NONE,           //空闲状态（安左键推线，中键移动画布）
	PUSH_LINE,      //推线状态
	MOVE_CANVAS,    //移动状态
	DELETE,         //删除状态
	ROTATE          //旋转状态
};
class AppConfig : public ConfigBase {
public:
	AppConfig() { m_name = QStringLiteral("AppConfig"); }
	~AppConfig() = default;
	//1.需要读取和写入的信息
	QString m_qFilePath = "./data";
	QString m_qFileName = "example.txt";

	//2.画笔相关信息
	int m_r = 2;  //实心点半径
	QPen m_defaultPen = QPen(QColor(65, 105, 225), 2);	//默认画笔
	QPen m_blackPen = QPen(Qt::black, 2);
	QPen m_resPen = QPen(Qt::green, 2);	//默认画笔

	//3.状态信息
	PainState painState = NONE;     //绘图模式
	//4.1视图选项对应的bool值
	bool m_flip_left_right = false;	//01.翻转X轴(左右)
	bool m_flip_up_down = true;		//02.翻转Y轴(上下)
	bool m_rotate_90 = false;		//03.旋转90度
	//4.2算法选项对应的bool值
	bool m_preViaAlctOn = false;	//11.预via分配
	bool m_GNDRouteOn = true;		//12.GND特殊布线
	bool m_VCCRouteOn = false;		//13.VCC特殊布线
	bool m_diffRouteOn = false;		//14差分布线
	bool m_4_8Tree = false;			//15.4/8方向搜索树
	bool m_autoPush = true;			//16.自动推线,几何重构
	bool m_autoWriteExpResult;      //17.自动写入实验结果
	bool m_onGrids = false;
	//4.3显示选项对应的bool值
	bool m_showObs1 = true;			//21.显示第1层障碍物
	bool m_showObs2 = true;		//22.显示第2层障碍物
	bool m_showFlyLines = false;		//23.显示飞线
	bool m_showPahts = true;		//24.显示布线结果
	bool m_showPPs = false;			//55.显示规划点
	bool m_show_vias = true;		//26.显示过孔
	bool m_showPinCenters = true;	//27.显示Pin中心
	//4.4其他选项对应的bool值
	bool m_postOn = true;			//51.启用后处理
	bool m_showTrees = false;		//52.显示树
	bool m_debugFuncOn = false;		//53.启用调试中断

	int m_directionOp = 2;			//0无，1四方向，2八方向
	int m_gridType = 0;				//0无网格，1线形网格，2点形网格
	int m_postMode = 2;
	int m_pushRunMode = 0;			//0.Off,1.Block

	float m_minimalScreenGridSize = 16;
	float m_gridSize = 0;

	int m_showTreeIndex = 0;

	//5.自定义的输入数据
	QString m_flexibleOpt = "";		//UI界面输入的字符串，可以代表不同模式选项等
	double m_doubleNum0 = 0.0;		//UI界面输入的小数，可代表坐标，以空格分界
	double m_doubleNum1 = 0.0;		//UI界面输入的小数
	int m_intNum2 = 0;				//UI界面输入的int树
	int m_intNum3 = 8;

	//5.配置信息操作
	void configUpdate(const QString& qFilePath, const QString& qFileName);
	bool read() override;
	bool save() const override;
};
