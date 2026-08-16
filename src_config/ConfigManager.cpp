#include "ConfigManager.h"
#include "AppConfig.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>

// 内置 default.config 模板
static const char* kDefaultConfigTemplate =
    "configPath ./config/configs\n";

// 内置 app.config 模板（与 config/app.config 文件内容保持一致）
static const char* kAppConfigTemplate =
    "defaltPath ./data\n"
    "viewOpt 0 1 0 \n"
    "algmOpt 0 0 0 0 0 1 0 \n"
    "dispOpt 1 1 0 1 0 1 1 \n"
    "boolOpt 1 0 0 \n"
    "m_directionOp 2\n"
    "m_gridType 0\n"
    "m_pushRunMode 0\n"
    "m_postMode 2\n"
    "m_minimalScreenGridSize 16\n"
    "m_gridSize 0\n"
    "m_showTreeIndex 0\n"
    "m_flexibleOpt 123456\n"
    "m_doubleNum 161 129 11 10\n";

ConfigManager::ConfigManager() = default;
ConfigManager::~ConfigManager() = default;

ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

AppConfig* ConfigManager::bootstrap() {
    // 1. 确保 ./config/ 存在
    QDir configDir("config");
    if (!configDir.exists()) {
        configDir.mkpath(".");
    }

    // 2. 读 config/default.config 取 configPath（文件缺失则从内置模板重建，键缺失用默认）
    QString configPath = "config/configs";  // 默认 ./config/configs
    QFile defaultFile("config/default.config");
    if (!defaultFile.exists()) {
        if (defaultFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&defaultFile);
            out << kDefaultConfigTemplate;
            defaultFile.close();
        }
    }
    if (defaultFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&defaultFile);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;
            if (line.startsWith("configPath")) {
                configPath = line.section(' ', 1).trimmed();
                if (configPath.isEmpty()) configPath = "config/configs";
                break;
            }
        }
        defaultFile.close();
    }

    // 3. 确保 app.config 存在（缺失从内置模板写出；default.config 已在步骤 2 重建）
    QFile appConfigFile("config/app.config");
    if (!appConfigFile.exists()) {
        if (appConfigFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&appConfigFile);
            out << kAppConfigTemplate;
            appConfigFile.close();
        }
    }

    // 4. 读 app_last.config；缺失 → 从 app.config 复制
    QFile lastFile("config/app_last.config");
    if (!lastFile.exists()) {
        appConfigFile.copy("config/app_last.config");
    }

    // 5. mkpath configPath 确保个人配置目录存在
    QDir(configPath).mkpath(".");

    // 6. 迁移：若 configPath 已不在 config/ 下，且 ./config/configs/ 仍存在且有内容
    //    → 整体移动到新 configPath 并删除 ./config/configs/
    {
        QDir oldConfigsDir("config/configs");
        QString configAbs = QDir(configPath).absolutePath();
        QString configRootAbs = QDir("config").absolutePath();
        bool underConfig = configAbs.startsWith(configRootAbs, Qt::CaseInsensitive);
        if (!underConfig && oldConfigsDir.exists()) {
            QStringList entries = oldConfigsDir.entryList(
                QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
            if (!entries.isEmpty()) {
                QDir(configPath).mkpath(".");
                for (const QString& entry : entries) {
                    QString srcPath = oldConfigsDir.absoluteFilePath(entry);
                    QString dstPath = QDir(configPath).absoluteFilePath(entry);
                    QFile::rename(srcPath, dstPath);
                }
                oldConfigsDir.removeRecursively();
            }
        }
    }

    // 7. 创建 AppConfig，加载并返回（由本单例内部持有）
    m_config = std::make_unique<AppConfig>();
    m_config->setFilePath("config/app_last.config");
    m_config->read();
    m_configPath = configPath;
    return m_config.get();
}

void ConfigManager::saveLast(AppConfig* config) {
    if (!config) return;
    config->setFilePath("config/app_last.config");
    config->save();
}
