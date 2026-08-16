#pragma once

#include <QWidget>
#include "ui_OptionsPanel.h"
#include "../../../src_config/AppConfig.h"
#include "../../PanelBase.h"
#include <QTabWidget>
#include <QCheckBox>
#include <QVBoxLayout>

class OptionsPanel : public PanelBase
{
	Q_OBJECT

signals: //信号
	//发送给父类的信号
	void update_request_signal();			// 更新绘图
	void tranform_init_request_signal();	// 初始化变换矩阵
	void pressMSTReset();					// 重新生成MST

	void open_res_request_signal();			// 打开Res文件进行查看
	void reset_request_signal();			// 复位重置
	void random_pin_request_signal();		// 重新生成随机pin
	void pin_decrease_request_signal();		// pin数量减少
	void pin_increase_request_signal();		// pin数量增加
	void save_case_request_signal();		// 保存case
	void save_image_request_signal();		// 保存图片
	void write_pre_request_signal();		// 保存预处理结果
	void write_res_request_signal();		// 在预处理结果后面追加结果
	void PPTT_run_request_signal();			// PPDT运行
	void poly_run_request_signal();			// 多边形算法运行

public slots:
	void updatePanelUI(int size);

private: //槽函数
	void paintChanged(int type = 0) {		// 绘图信息变化时触发，请求绘图更新
		switch (type) {
		case 0:
			emit update_request_signal();
			break;
		case 1:
			emit tranform_init_request_signal();
			break;
		case 2:
			emit pressMSTReset();
			break;
		default:
			break;
		}
	}

public:
	explicit OptionsPanel(AppConfig* conf, QWidget* parent);
	~OptionsPanel();
	void init() override {}

private:
	Ui::OptionsPanelClass ui;
	AppConfig* m_config;			//配置信息

private:
	void styleInit();
	void setPanelUI();
	void setup_panelSlots();
};
