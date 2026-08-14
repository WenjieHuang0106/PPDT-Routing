#include "TabPage.h"
#include "qfileinfo.h"


TabPageFactory& TabPageFactory::instance() {
    static TabPageFactory factory;
    return factory;
}

void TabPageFactory::registerCreator(const QString& suffix, CreatorFunc creator) {
    m_funcs[suffix.toLower()] = creator;
}

TabPage* TabPageFactory::create(const QString& qFullName, QWidget* parent) {
    QFileInfo fileInfo(qFullName);
	QString path = fileInfo.absolutePath();
	QString filename = fileInfo.fileName();
    QString suffix = fileInfo.suffix().toLower();

    QString key = suffix.toLower();
    if (m_funcs.contains(key)) {
        TabPage* page = m_funcs[key](qFullName, path, filename, parent);
        return page;
    }
    return nullptr; // 没有找到对应的创建器
}