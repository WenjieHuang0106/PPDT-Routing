#include "src_ui/GeoDisplay.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    QApplication app(argc, argv);
    GeoDisplay window;
    window.setWindowTitle("PPDT and pushLine");
    window.resize(1000, 600);
    window.show();
    return app.exec();
}
