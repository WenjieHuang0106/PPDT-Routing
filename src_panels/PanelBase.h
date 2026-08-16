#pragma once
#include <QWidget>
#include <QString>

class PanelBase : public QWidget {
    Q_OBJECT
public:
    explicit PanelBase(const QString& title, QWidget* parent = nullptr);
    virtual ~PanelBase() = default;
    QString title() const { return m_title; }
    Qt::DockWidgetArea area() const { return m_area; }
    bool isHidden() const { return m_hidden; }
    void setArea(Qt::DockWidgetArea a) { m_area = a; }
    void setHidden(bool h) { m_hidden = h; }
    virtual void init() = 0;
protected:
    QString m_title;
    Qt::DockWidgetArea m_area = Qt::LeftDockWidgetArea;
    bool m_hidden = false;
};
