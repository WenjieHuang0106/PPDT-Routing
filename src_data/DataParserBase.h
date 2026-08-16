#pragma once
#include "src_data/DataBase.h"
#include <QString>
#include <QDir>
#include <QFileInfo>

//解析器基类：具体文件格式的解析器继承此类（见 src_data/dsn/）
class DataParserBase {
public:
	DataParserBase(){};
	virtual ~DataParserBase() = default;

	virtual bool readFile(const QString& qFullName) = 0;
	virtual bool saveFile(const QString& qFullName) = 0;
	virtual bool saveFileAs(const QString& qFullName) = 0;
private:
	bool x_horizontal = false;	//0表示xy不交换，1表示xy交换

protected:
	virtual void setMinMax() = 0;

};
