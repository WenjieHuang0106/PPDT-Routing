#pragma once
#include"../../src_baseClasses/DataParser.h"
#include "Data_dsn.h"
#include <vector>
#include <QString>

#include <QFile>
#include <QTextStream>

class DataParser_dsn : public DataParser {
public:
	DataParser_dsn(Data_dsn* data) : m_data(data) {};
	~DataParser_dsn() {};

	//实例化基类虚函数
	bool readFile(const QString& qFullName);
	bool saveFile(const QString& qFullName);
	bool saveFileAs(const QString& qFullName);
	void setMinMax();
	void parseDSNData(std::vector<QString>& lines, int type);

private:
	Data_dsn* m_data;

	QHash<QString, int> m_layerNameToId;	//层名转层ID映射关系
	// DSN解析辅助函数
	//************
	void parseDSNParser(std::vector<QString>& lines);
	void parseDSNResolution(std::vector<QString>& lines);
	void parseDSNStructure(std::vector<QString>& lines);
	void parseDSNPlacement(std::vector<QString>& lines);
	void parseDSNLibrary(std::vector<QString>& lines);
	void parseDSNNetwork(std::vector<QString>& lines);
	void parseDSNWiring(std::vector<QString>& lines);

	//************
	void parse_struc_boundary(std::vector<QString>& lines, int& i);
	void parse_struc_via(std::vector<QString>& lines, int& i);
	void parse_struc_grid(std::vector<QString>& lines, int& i);
	void parse_struc_rule(std::vector<QString>& lines, int& i);
	void parse_struc_layer(std::vector<QString>& lines, int& i);
	void parse_lib_pin(std::vector<QString>& lines, int& i);
	void parse_lib_pad(std::vector<QString>& lines, int& i);
	void parse_network_net(std::vector<QString>& lines, int& i);
	void parse_network_class(std::vector<QString>& lines, int& i);
	void parse_network_class_circuit(const std::vector<QString>& lines, const QString& shapeName, int& i, int& bracketCount);
	void parse_network_class_rule(const std::vector<QString>& lines, const QString& shapeName, int& i, int& bracketCount);
	void parse_wiring_wire(std::vector<QString>& lines, int& i);
	void parse_wiring_via(std::vector<QString>& lines, int& i);

	void updateBracketCount(const QString& line, int& bracketCount);


	static void extractAllNumbers(std::vector<double>& out, const QString& s);

};