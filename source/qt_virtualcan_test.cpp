#include <QCoreApplication>
#include <QCanBus>
#include <QCanBusDevice>
#include <QCanBusFrame>
#include <QTimer>
#include <iostream>
#include <thread>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString errorString;
    QCanBusDevice* device =
        QCanBus::instance()->createDevice("virtualcan", "can0", &errorString);

    if (!device)
    {
        std::cerr << "Failed to create Qt CAN device: "
                  << errorString.toStdString() << std::endl;
        return 1;
    }

    std::thread::id mainThread = std::this_thread::get_id();

    QObject::connect(device, &QCanBusDevice::framesReceived, [&]() {

        if (std::this_thread::get_id() != mainThread) {
            std::cerr << "THREAD MISMATCH in framesReceived!\n";
            std::abort();
        }
        while (device->framesAvailable())
        {
            auto frame = device->readFrame();
            std::cout << "[Qt] Received frame ID=" << frame.frameId()
                      << " Payload=" << frame.payload().toHex().toStdString()
                      << std::endl;
        }
    });

    if (!device->connectDevice())
    {
        std::cerr << "Failed to connect Qt CAN device" << std::endl;
        return 1;
    }

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&]() {

        if (std::this_thread::get_id() != mainThread) {
            std::cerr << "THREAD MISMATCH in timer!\n";
            std::abort();
        }
        QByteArray payload;
        payload.append(char(0x11));
        payload.append(char(0x22));
        payload.append(char(0x33));

        QCanBusFrame frame(0x123, payload);
        if (!device->writeFrame(frame)) {
            std::cerr << "writeFrame FAILED\n";
            std::abort();
        }

        std::cout << "[Qt] Sent frame ID=0x123" << std::endl;
    });

    QObject::connect(device, &QCanBusDevice::stateChanged,
        [](QCanBusDevice::CanBusDeviceState state)
    {
    std::cout << "CAN STATE: " << state << std::endl;

    if (state != QCanBusDevice::ConnectedState)
    {
    std::cerr << "CAN DISCONNECTED!\n";
    }
    });

    timer.start(1000);

    return app.exec();
}
