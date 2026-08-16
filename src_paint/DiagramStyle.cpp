#include "DiagramStyle.h"
#include <QMessageBox>
PointStyle* DiagramStyleManager::getPointStyle(const QString& key){
	if (m_pointStyles.contains(key))
		return &m_pointStyles[key];
	return &m_pointStyles[""];
};
const PointStyle* DiagramStyleManager::getPointStyle(const QString& key) const {
	auto it = m_pointStyles.constFind(key);
	if (it != m_pointStyles.constEnd())
		return &it.value();
	it = m_pointStyles.constFind(QStringLiteral(""));
	return &it.value();
};
LineStyle* DiagramStyleManager::getLineStyle(const QString& key) {
	if (m_lineStyles.contains(key))
		return &m_lineStyles[key];
	return &m_lineStyles[""];
};
const LineStyle* DiagramStyleManager::getLineStyle(const QString& key) const {
	auto it = m_lineStyles.constFind(key);
	if (it != m_lineStyles.constEnd())
		return &it.value();
	it = m_lineStyles.constFind(QStringLiteral(""));
	return &it.value();
};
PolygonStyle* DiagramStyleManager::getPolygonStyle(const QString& key) {
	if (m_polygonStyles.contains(key))
		return &m_polygonStyles[key];
	return &m_polygonStyles[""];
};
const PolygonStyle* DiagramStyleManager::getPolygonStyle(const QString& key) const {
	auto it = m_polygonStyles.constFind(key);
	if (it != m_polygonStyles.constEnd())
		return &it.value();
	it = m_polygonStyles.constFind(QStringLiteral(""));
	return &it.value();
};

//设置样式
void DiagramStyleManager::setPointStyle(const QString& key, const PointStyle& style) {
	m_pointStyles[key] = style;
}
void DiagramStyleManager::setPointStyle(const std::vector<QString>& keys, const std::vector<PointStyle>& styles) {
	if (styles.size() != keys.size())
        QMessageBox::warning(nullptr, "样式设置失败", "PointStyles设置失败");
	for (int i = 0; i < keys.size(); i++)
		m_pointStyles[keys[i]] = styles[i];
}
void DiagramStyleManager::setLineStyle(const QString& key, const LineStyle& style) {
	m_lineStyles[key] = style;
}
void DiagramStyleManager::setLineStyle(const std::vector<QString>& keys, const std::vector<LineStyle>& styles) {
	if (styles.size() != keys.size())
		QMessageBox::warning(nullptr, "样式设置失败", "LineStyles设置失败");
	for (int i = 0; i < keys.size(); i++)
		m_lineStyles[keys[i]] = styles[i];
}
void DiagramStyleManager::setPolygonStyle(const QString& key, const PolygonStyle& style) {
	m_polygonStyles[key] = style;
}
void DiagramStyleManager::setPolygonStyle(const std::vector<QString>& keys, const std::vector<PolygonStyle>& styles) {
    if (styles.size() != keys.size())
		QMessageBox::warning(nullptr, "样式设置失败", "PolygonStyles设置失败");
    for (int i = 0; i < keys.size(); i++)
        m_polygonStyles[keys[i]] = styles[i];
}
