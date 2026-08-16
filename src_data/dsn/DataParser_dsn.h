#pragma once
#include "../DataParserBase.h"
#include "Data_dsn.h"
#include "../../src_db/dsn/dbParser_dsn.h"
#include <memory>
#include <vector>
#include <QString>

// 前端 DSN 解析器：负责把 Qt 路径传给内核解析器，并在解析成功后
// 触发前端数据的 transform(单位转换) + setPaintData(构建 UI 绘图数据)。
class DataParser_dsn : public DataParserBase {
public:
	explicit DataParser_dsn(Data_dsn* data)
	    : m_frontendData(data)
	    , m_dbParser(nullptr)  // 稍后在 readFile 中绑定到 m_dbDataBuf
	{}
	~DataParser_dsn() override = default;

	// 对外接口
	bool readFile(const QString& qFullName) override;
	bool saveFile(const QString& qFullName) override;
	bool saveFileAs(const QString& qFullName) override;

	// 计算前端 minPt/maxPt 缓存（从内核 m_bbox 同步），
	// 保留虚函数以兼容 DataParserBase。
	void setMinMax() override;

private:
	Data_dsn* m_frontendData = nullptr;
	std::unique_ptr<db::dsn::Data_dsn> m_dbDataBuf;
	db::dsn::Parser_dsn m_dbParser;  // 注意：Parser_dsn 需要 Data_dsn*，
	                                 // 所以我们每次 readFile 前用 placement-new 重新构造。
};
