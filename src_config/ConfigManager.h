#pragma once
#include <QString>
#include <memory>

class AppConfig;

// 配置管理器（单例）：负责启动引导、维护 config/ 目录结构、持有 AppConfig
class ConfigManager {
public:
    static ConfigManager& instance();
    ~ConfigManager();

    // 执行 §五 的 7 步启动引导，返回加载好的 AppConfig（由本单例内部持有）
    AppConfig* bootstrap();

    // 写 config/app_last.config
    void saveLast(AppConfig* config);

    // 返回当前个人配置目录路径
    QString configPath() const { return m_configPath; }

private:
    ConfigManager();
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    std::unique_ptr<AppConfig> m_config;
    QString m_configPath;
};
