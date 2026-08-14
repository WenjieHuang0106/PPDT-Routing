#pragma once
#include "Data.h"
#include <QString>
#include <QDir>
#include <QFileInfo>

class DataParser{
public:
	DataParser(){};
	virtual ~DataParser() = default;

	virtual bool readFile(const QString& qFullName) = 0;
	virtual bool saveFile(const QString& qFullName) = 0;
	virtual bool saveFileAs(const QString& qFullName) = 0;
private:
	bool x_horizontal = false;	//0表示xy不交换，1表示xy交换

protected:
	virtual void setMinMax() = 0;

};

