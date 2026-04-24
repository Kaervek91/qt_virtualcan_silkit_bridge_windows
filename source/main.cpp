#include <QApplication>
#include <iostream> 
#include "canbridge.h"



int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    /*MainWindow w;
    w.show();*/
    CanBridge bridge;

    return app.exec();
}
