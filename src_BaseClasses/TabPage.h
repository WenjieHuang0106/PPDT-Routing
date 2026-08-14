#pragma once
#include <QWidget>
#include "../src_config/configUI.h"
//#include "../src_algorithms/AlgorithmSet.h"
#include "../src_paint/Diagram.h"

class Data;
class DataParser;

class TabPage : public QWidget {
	Q_OBJECT
public:
	explicit TabPage(QString qFullName, QString qFilePath, QString qFileName, QWidget* parent = nullptr)
		: m_qFullName(qFullName), 
		m_qFilePath(qFilePath), 
		m_qFileName(qFileName), 
		QWidget(parent),
		m_diagram(nullptr){
	}
	virtual ~TabPage() = default;

	QString fullName() const { return m_qFullName; };
	QString filePath() const { return m_qFilePath; };
	QString fileName() const { return m_qFileName; };
	//AlgorithmSet* m_alg;

protected:
	QString m_qFullName = "data/example.map";	//文件全路径，含文件名
	QString m_qFilePath = "data";				//文件路径，不含文件名
	QString m_qFileName = "example.map";		//文件名带后缀，仅名称
	QString m_qFullSaveName = "0001.map";
	bool m_dataAcquired = false;				//记录是否成功获取数据，成功为true
	bool m_runFinished = false;
	Diagram* m_diagram;          //绘图窗口

public:
	void pageInit(ConfigUI* conf) {
		fillData(conf);
		conf->configUpdate(m_qFilePath, m_qFileName);
		setupConnections();
	}
private:
	void setupConnections() const {
		connect(m_diagram, &Diagram::leftPressed,
			this, &TabPage::leftPressRun);
		connect(m_diagram, &Diagram::leftPressedMove,
			this, &TabPage::leftPressMoveRun);
		connect(m_diagram, &Diagram::leftReleaseed,
			this, &TabPage::leftReleaseRun);
	}
	
public: // 槽函数
	virtual void action_reset() = 0;					// 复位重置（仅针对读取map类的txt文件）
	virtual void action_random_pin() = 0;			// 重新生成随机pin
	virtual void pin_decrease() = 0;					// pin数量减少
	virtual void action_pin_increase() = 0;			// pin数量增加
	virtual void action_save_case() = 0;				// 保存case
	virtual void action_save_image() = 0;			// 保存图片
	virtual void action_write_pre() = 0;				// 保存预处理结果
	virtual void action_write_res() = 0;				// 在预处理结果后面追加结果

	virtual void pressRunButton() = 0;
	virtual void onConfigChanged(int type = 0) = 0;
	virtual void tranformChanged() = 0;
	virtual void leftPressRun(SelectedTarget* target) = 0;
	virtual void leftPressMoveRun(SelectedTarget* target) = 0;
	virtual void leftReleaseRun(SelectedTarget* target) = 0;
protected:
	virtual void fillData(ConfigUI* conf) = 0;
	virtual void refreshUI() = 0;
signals:
	void requestUpdatePanelUI(int size);
	
};

class TabPageFactory {
public:
	using CreatorFunc = std::function<TabPage* (const QString& qFullName, const QString& qFilePath, const QString& qFileName, QWidget* parent)>;
	static TabPageFactory& instance();
	// 注册某个后缀对应的 TabPage 构造函数
	void registerCreator(const QString& suffix, CreatorFunc creator);	
	TabPage* create(const QString& qFullName, QWidget* parent);

private:
	TabPageFactory() = default;
	QHash<QString, CreatorFunc> m_funcs;
};