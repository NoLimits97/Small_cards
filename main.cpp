#include "TZ_Project.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    TZ_Project window;
    window.show();
    return app.exec();
}
