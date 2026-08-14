#pragma once
#include <QWidget>
#include "AlgorithmLink/AlgorithmLink_dsn.h"
#include "../../src_baseClasses/TabPage.h"
#include "../../src_config/configUI.h"
#include "../../src_config/DiagramStyle.h"
#include "DataParser_dsn.h"

class TabPage_dsn : public TabPage
{
    Q_OBJECT
public:
    TabPage_dsn(QString qFullName, QString qFilePath, QString qFileName, QWidget* parent)
        : TabPage(qFullName, qFilePath, qFileName, parent) {
		m_data = new Data_dsn();
		m_parser = new DataParser_dsn(m_data);
    }

    ~TabPage_dsn() {
        delete m_algm;
        delete m_parser;
		delete m_data;
	}
private:
    Data_dsn* m_data;;	        //数据对象
    DataParser_dsn* m_parser;	//数据解析器
    AlgorithmLink_dsn* m_algm = nullptr;

private:
    ConfigUI* m_config = nullptr;	//配置信息
    void fillData(ConfigUI* conf);
    void refreshUI();
public:
    void action_reset() {};				// 复位重置（仅针对读取map类的txt文件）
    void action_random_pin() {};			// 重新生成随机pin
    void pin_decrease() {};				// pin数量减少
    void action_pin_increase() {};			// pin数量增加
    void action_save_case() {};			// 保存case
    void action_save_image() {};			// 保存图片
    void action_write_pre() {};			// 保存预处理结果
    void action_write_res() {};			// 在预处理结果后面追加结果
    void pressRunButton();

    void onConfigChanged(int type = 0);
    void tranformChanged();
    void leftPressRun(SelectedTarget* target);
    void leftPressMoveRun(SelectedTarget* target);
    void leftReleaseRun(SelectedTarget* target);

};