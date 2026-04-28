#pragma once

#include <QObject>
#include <QCanBus>
#include <iostream>
// SIL Kit
#include "silkit/SilKit.hpp"
#include "silkit/services/can/all.hpp"
#include "silkit/services/orchestration/all.hpp"

#include <queue>
#include <mutex>
#include <QThread>
#include <vector>
#include <array>


#define CHECK_THREAD(name) \
if (QThread::currentThread() != thread()) { \
    std::cerr << name << " THREAD VIOLATION\n"; \
    std::terminate(); \
}

struct CanMessage {
    uint32_t id;
    std::vector<uint8_t> data;
};


class CanBridge : public QObject
{
    Q_OBJECT

public:
    explicit CanBridge(const int& port, QObject *parent = nullptr)
        : QObject(parent)
    {
        std::cout << "CanBridge class instatiated" << std::endl;
        if (QCanBus::instance()->plugins().contains(QStringLiteral("virtualcan"))) {
            // plugin available
            std::cout << "QTCANBUS VIRTUAL CAN PRESENT" << std::endl;
            // TO BE IGNORED QT 
            blockedIds = {
                0x06,0x07,0x08,0x09,0x0A,0x0B,0x0F,0x10,0x11,0x12,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1D,0x1E,0x1F,0x26,0xFF,0x101
            };

            QString error;
            qtCanDevice = QCanBus::instance()->createDevice("virtualcan", "can0", &error);

            if (!qtCanDevice)
            {
                std::cerr << "Qt CAN init error: " << error.toStdString() << std::endl; } //return; 

            if (!qtCanDevice->connectDevice()) {  std::cerr << "Qt CAN connect error\n"; } //return; 
            
            std::cout << "Connected to Qt virtual CAN\n";
            std::cout << "CanBridge created in thread: "  << QThread::currentThread() << std::endl;
            std::cout << "qtCanDevice thread: " << qtCanDevice->thread() << std::endl;
            
            QObject::connect(qtCanDevice, &QCanBusDevice::framesReceived,
                this, &CanBridge::onQtFramesReceived,
                Qt::QueuedConnection);
            try {
                // Create a SIL Kit Participant
                auto config = SilKit::Config::ParticipantConfigurationFromString(R"(
                    Description: QtBridge
                    )");
                std::string registryUri = "silkit://localhost:" + std::to_string(port);
    
                participant = SilKit::CreateParticipant(config, "QtBridgeParticipant", registryUri);
                silCanController = participant->CreateCanController("CAN1", "CAN1");;
    
                silCanController->AddFrameHandler(
                    [this](auto*, const SilKit::Services::Can::CanFrameEvent& event)
                    {
                        if (shuttingDown) return;
                        QByteArray payload(
                            reinterpret_cast<const char*>(event.frame.dataField.data()),
                            static_cast<int>(event.frame.dlc));
    
                        uint32_t id = event.frame.canId;
                
                        QMetaObject::invokeMethod(this, [this, id, payload]()
                        {
                            QCanBusFrame qtFrame(id, payload);
                
                            if (qtCanDevice &&
                                qtCanDevice->state() == QCanBusDevice::ConnectedState)
                            {
                                qtCanDevice->writeFrame(qtFrame);
                                
                                std::cout << "SILKIT to QT bus: " 
                                        << qtFrame.frameId() << std::endl;
    
                                QByteArray data = qtFrame.payload();
    
                                std::cout << "SILKIT SENT to QT bus [" 
                                        << data.size() << " bytes]: ";
    
                                for (int i = 0; i < data.size(); ++i) {
                                    std::cout << static_cast<int>(static_cast<uint8_t>(data[i])) << " ";
                                }
    
                                std::cout << std::endl;
                            }
                        }, Qt::QueuedConnection);
                    });
                auto lifecycle = participant->CreateLifecycleService(
                    SilKit::Services::Orchestration::LifecycleConfiguration{
                        SilKit::Services::Orchestration::OperationMode::Autonomous
                    });
                
                lifecycle->StartLifecycle();
                silCanController->Start();

            } catch (const std::exception& e)
            {
                std::cerr << "Error conectando a SIL Kit:" << e.what();
                if (qtCanDevice)
                {
                    qtCanDevice->disconnectDevice();
                    delete qtCanDevice;
                    qtCanDevice = nullptr;
                }
                QString err = QString("SIL Kit connection failed: %1").arg(e.what());
                
                shuttingDown = true;
                return;
            }
                

        } else {
            std::cerr << "ERROR QTCANBUS VIRTUAL CAN NOT PRESENT: " << std::endl;
            //return;
        }
    }

    ~CanBridge()
    {
        std::cout << "Closing CanBridge..." << std::endl;
        shuttingDown = true;
        if (qtCanDevice)
        {
            qtCanDevice->disconnectDevice();
            qtCanDevice->deleteLater();
            qtCanDevice = nullptr;
        }

        silCanController = nullptr;
        participant.reset();

        std::cout << "CanBridge closed." << std::endl;
    }


private:
    QCanBusDevice* qtCanDevice{nullptr};
    SilKit::Services::Can::ICanController* silCanController{nullptr};
    std::shared_ptr<SilKit::IParticipant> participant;
    std::array<uint8_t, 128> txBuffer{};
    std::atomic<bool> shuttingDown{false};
    std::unordered_set<uint32_t> blockedIds;
    
    bool shouldIgnore(uint32_t id)
    {
        return blockedIds.find(id) != blockedIds.end();
    }
    void onQtFramesReceived()
    {
        int cnt = 0;
        // Empty Backlog last 20 frames
        int total = qtCanDevice->framesAvailable();
        int keepLast = 20;
        int discard = std::max(0, total - keepLast);

        for (int i = 0; i < discard; ++i)
        {
            qtCanDevice->readFrame();
        }

        while (qtCanDevice->framesAvailable() && cnt < keepLast){

            QCanBusFrame frame = qtCanDevice->readFrame();
            
            SilKit::Services::Can::CanFrame silFrame;
            silFrame.canId = frame.frameId();
            if (shouldIgnore(frame.frameId())) continue;
            silFrame.dlc = static_cast<uint8_t>(frame.payload().size());
            QByteArray payload = frame.payload();
            //std::array<uint8_t, 64> buffer{};
            std::copy(payload.begin(),
            payload.end(),
            txBuffer.begin());

            silFrame.dataField = SilKit::Util::Span<const uint8_t>(
                txBuffer.data(),
                silFrame.dlc
            );
            if (!silCanController || shuttingDown) return;
            silCanController->SendFrame(silFrame);

            std::cout << "Qt -> SIL Kit: ID=" << frame.frameId() << std::endl;
            cnt++;
            
        }

    }
};
