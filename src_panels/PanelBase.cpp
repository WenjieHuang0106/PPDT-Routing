#include "PanelBase.h"

PanelBase::PanelBase(const QString& title, QWidget* parent)
    : QWidget(parent)
    , m_title(title)
{
}
