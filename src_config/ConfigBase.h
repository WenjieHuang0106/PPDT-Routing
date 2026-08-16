#pragma once
#include <QString>
// 配置基类：所有配置类（如 AppConfig）继承此类
class ConfigBase {
public:
    virtual ~ConfigBase() = default;
    QString name() const { return m_name; }
    QString filePath() const { return m_filePath; }
    void setFilePath(const QString& p) { m_filePath = p; }
    virtual bool read() = 0;
    virtual bool save() const = 0;
protected:
    QString m_name;
    QString m_filePath;
};
