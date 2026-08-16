#include "DataParser_dsn.h"
#include <QFile>
#include <QDebug>

bool DataParser_dsn::readFile(const QString& qFullName) {
	if (!m_frontendData) return false;

	// 清空 UI 容器
	m_frontendData->clear();

	// 构造全新内核数据 buffer
	m_dbDataBuf = std::make_unique<db::dsn::Data_dsn>();

	// 绑定内核解析器到新的 buffer
	m_dbParser.bind(m_dbDataBuf.get());

	// 路径：QString → 本地编码的窄字符串（Windows 下支持 ANSI 路径）
	// 使用 QFile::encodeName 以保证非 ASCII 路径尽量能打开。
	QByteArray localName = QFile::encodeName(qFullName);
	std::string narrowPath(localName.constData(), localName.size());

	// 调用内核解析
	bool ok = m_dbParser.readFile(narrowPath);
	if (!ok) {
		qDebug() << "[DataParser_dsn] 内核解析失败:" << qFullName;
		m_dbDataBuf.reset();
		return false;
	}

	// 内核完成后：按 resolution 做单位转换。
	// 原逻辑 (m_trans 默认=1) 不做 mil->mm 转换，直接显示原始 mil 尺度，
	// 这样鼠标显示坐标、pin/pad 坐标与 DSN 文件的数字一致（都是几千的 mil 值）。
	// 如需切换到 mm，让用户通过设置改变 m_trans，这里默认与旧代码保持一致。
	double scale = 1.0;
	(void)m_dbDataBuf->m_resolution; // 预留，目前不主动做单位缩放
	m_dbDataBuf->applyScale(scale);  // 内部重算 m_bbox (scale=1 直接跳过)

	// 关联到前端数据（m_dbDataBuf 所有权转移给 shared_ptr）
	m_frontendData->attachDbData(std::move(m_dbDataBuf));

	// 从内核数据构建 UI 绘图容器
	m_frontendData->setPaintData();

	return true;
}

bool DataParser_dsn::saveFile(const QString&) { return true; }
bool DataParser_dsn::saveFileAs(const QString&) { return true; }

void DataParser_dsn::setMinMax() {
	// minPt/maxPt 已由 getMinPoint()/getMaxPoint() 在调用时从内核 bbox 动态同步。
	// 此处虚函数保留空实现，兼容 DataParserBase 基类接口。
}
