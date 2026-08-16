#include "src_ui/GeoDisplay.h"
#include <QtWidgets/QApplication>
#include <QtCore/QDir>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // 锁定工作目录到 exe 所在目录，确保 data/、config/ 等相对路径无论从哪里启动都能正确找到
    QDir::setCurrent(QCoreApplication::applicationDirPath());
    GeoDisplay window;
    window.setWindowTitle("PPDT and pushLine");
    window.resize(1000, 600);
    window.show();
    return app.exec();
}
