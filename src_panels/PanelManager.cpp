#include "PanelManager.h"
#include "PanelBase.h"
#include <QMainWindow>
#include <QDockWidget>
#include <QMenu>
#include <QAction>

PanelManager& PanelManager::instance() {
    static PanelManager inst;
    return inst;
}

void PanelManager::registerPanel(PanelBase* panel) {
    if (!panel)
        return;
    PanelEntry entry;
    entry.panel = panel;
    entry.dock = nullptr;
    m_panels.insert(panel->title(), entry);
}

void PanelManager::attachTo(QMainWindow* mainWnd) {
    m_mainWnd = mainWnd;
    if (!mainWnd)
        return;
    for (auto it = m_panels.begin(); it != m_panels.end(); ++it) {
        PanelEntry& entry = it.value();
        if (!entry.panel || entry.dock)
            continue;
        QDockWidget* dock = new QDockWidget(entry.panel->title(), mainWnd);
        dock->setWidget(entry.panel);
        mainWnd->addDockWidget(entry.panel->area(), dock);
        dock->setVisible(!entry.panel->isHidden());
        entry.dock = dock;
    }
}

QDockWidget* PanelManager::dockFor(const QString& title) const {
    auto it = m_panels.find(title);
    if (it == m_panels.end())
        return nullptr;
    return it.value().dock;
}

QMenu* PanelManager::buildWindowMenu(QWidget* parent) {
    QMenu* menu = new QMenu(QStringLiteral("窗口"), parent);
    for (auto it = m_panels.begin(); it != m_panels.end(); ++it) {
        PanelEntry& entry = it.value();
        if (!entry.panel)
            continue;
        QAction* action = new QAction(entry.panel->title(), menu);
        action->setCheckable(true);
        bool visible = entry.dock ? entry.dock->isVisible() : !entry.panel->isHidden();
        action->setChecked(visible);
        QDockWidget* dock = entry.dock;
        // action -> dock: 勾选=显示, 取消=隐藏
        QObject::connect(action, &QAction::toggled, menu, [dock](bool checked) {
            if (dock)
                dock->setVisible(checked);
        });
        // dock -> action: 双向绑定 dock 显隐状态到 QAction 勾选
        if (dock) {
            QObject::connect(dock, &QDockWidget::visibilityChanged, menu, [action](bool v) {
                action->setChecked(v);
            });
        }
        menu->addAction(action);
    }
    return menu;
}
