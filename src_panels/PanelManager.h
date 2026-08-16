#pragma once
#include <QHash>
#include <QString>
#include <QWidget>

class QMainWindow;
class QMenu;
class QDockWidget;
class PanelBase;

class PanelManager {
public:
    static PanelManager& instance();
    void registerPanel(PanelBase* panel);
    void attachTo(QMainWindow* mainWnd);
    QMenu* buildWindowMenu(QWidget* parent);
    // 返回指定标题面板对应的 dock（未挂载时返回 nullptr）
    QDockWidget* dockFor(const QString& title) const;

private:
    PanelManager() = default;
    struct PanelEntry {
        PanelBase* panel = nullptr;
        QDockWidget* dock = nullptr;
    };
    QHash<QString, PanelEntry> m_panels;
    QMainWindow* m_mainWnd = nullptr;
};
