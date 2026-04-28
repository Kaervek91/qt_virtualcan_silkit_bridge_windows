#include <QApplication>
#include <iostream> 
#include "canbridge.h"
#include <csignal>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QString>

void signalHandler(int)
{
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        []() {
            std::cout << "Ctrl+C detected, quitting..." << std::endl;
            QCoreApplication::quit();
        },
        Qt::QueuedConnection);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    std::signal(SIGINT, signalHandler);
    QCommandLineParser parser;
    parser.setApplicationDescription("QT SIL KIT CAN Bridge application");
    parser.addHelpOption();

    QCommandLineOption portOption(
        QStringList() << "p" << "port",
        "Port for SIL Kit Registry",
        "port",
        "8500"
    );

    parser.addOption(portOption);
    parser.process(app);

    int port = parser.value(portOption).toInt();

    CanBridge bridge(port);

    return app.exec();
}
