#pragma once

#include <QObject>
#include <QCanBus>
#include <iostream>
#include <QTimer>
// SIL Kit
#include "silkit/SilKit.hpp"
#include "silkit/services/can/all.hpp"
#include "silkit/services/orchestration/all.hpp"

#include <queue>
#include <mutex>
#include <QThread>
#include <vector>
#include <array>

#include <unordered_set>
#include <chrono>
#include <deque>
#include <functional>

#define CHECK_THREAD(name) \
if (QThread::currentThread() != thread()) { \
    std::cerr << name << " THREAD VIOLATION\n"; \
    std::terminate(); \
}

template<typename T>
class SafeQueue {
public:
    void push(const T& v) {
        std::lock_guard<std::mutex> lock(m);
        q.push(v);
    }

    bool pop(T& v) {
        std::lock_guard<std::mutex> lock(m);
        if (q.empty()) return false;
        v = q.front();
        q.pop();
        return true;
    }

private:
    std::queue<T> q;
    std::mutex m;
};

enum Source {
    QtApp,
    SilKitBridge
};

struct CanMessage {
    uint32_t id;
    std::vector<uint8_t> data;
    Source source;
    uint64_t timestamp;
    uint64_t hash;
};

class CanBridge : public QObject
{
    Q_OBJECT

public:
    explicit CanBridge(QObject *parent = nullptr)
        : QObject(parent)
    {
        std::cout << "CanBridge class instatiated" << std::endl;
        
        std::cout << "THREAD QtBridge constructor: "
          << QThread::currentThreadId() << std::endl;
        

        if (QCanBus::instance()->plugins().contains(QStringLiteral("virtualcan"))) {
            // plugin available
            std::cout << "QTCANBUS VIRTUAL CAN PRESENT" << std::endl;
            
            QString error;
            qtCanDevice = QCanBus::instance()->createDevice("virtualcan", "can0", &error);

            if (!qtCanDevice)
            {
                std::cerr << "Qt CAN init error: " << error.toStdString() << std::endl;
                //return;
            }

            if (!qtCanDevice->connectDevice())
            {
                std::cerr << "Qt CAN connect error\n";
                //return;
            }
            
            std::cout << "Connected to Qt virtual CAN\n";

            std::cout << "CanBridge created in thread: "
              << QThread::currentThread() << std::endl;

            std::cout << "qtCanDevice thread: "
                    << qtCanDevice->thread() << std::endl;
            
            QObject::connect(qtCanDevice, &QCanBusDevice::framesReceived,
                this, &CanBridge::onQtFramesReceived,
                Qt::QueuedConnection);
            
            QObject::connect(qtCanDevice, &QCanBusDevice::stateChanged,
                    this, [](QCanBusDevice::CanBusDeviceState state){
                std::cout << "Qt CAN state: " << state << std::endl;
            });

            QObject::connect(qtCanDevice, &QCanBusDevice::errorOccurred,
                    this, [](QCanBusDevice::CanBusError err){
                std::cerr << "Qt CAN error: " << err << std::endl;
            });

            QTimer* debugTimer = new QTimer(this);
            connect(debugTimer, &QTimer::timeout, this, [](){
                std::cout << "Qt alive tick" << std::endl;
            });
            debugTimer->start(1000);
            // Create a SIL Kit Participant
            auto config = SilKit::Config::ParticipantConfigurationFromString(R"(
            Description: "Qt to SIL Kit Bridge")");

            participant = SilKit::CreateParticipant(config, "QtBridgeParticipant", "silkit://localhost:8500");
            participant->StartLifecycle();
            silCanController = participant->CreateCanController("CAN1", "CAN1");;

            silCanController->AddFrameHandler(
                [this](SilKit::Services::Can::ICanController*, const SilKit::Services::Can::CanFrameEvent& event)
                {
                    CanMessage msg;
                    msg.id = event.frame.canId;
                    msg.data.assign(event.frame.dataField.begin(),
                                    event.frame.dataField.end());
                    msg.source = Source::SilKitBridge;
            
                    silToQtQueue.push(msg);
            
                    QMetaObject::invokeMethod(this, [this]() {
                        processSilQueue();
                    }, Qt::QueuedConnection);
                });
            
            
            // silCanController->AddFrameHandler(
            //     [this](SilKit::Services::Can::ICanController*, const SilKit::Services::Can::CanFrameEvent& event) {
            //         onSilKitFrameReceived(event);
            //     });
            
            
            silCanController->Start();
            running = true;
            silThread = std::thread(&CanBridge::silKitLoop, this);
            

        } else {
            std::cerr << "ERROR QTCANBUS VIRTUAL CAN NOT PRESENT: " << std::endl;
            //return;
        }
    }

    ~CanBridge(){
        running = false;

        if (silThread.joinable())
            silThread.join();

        // flush opcional de colas
        CanMessage msg;

        while (qtToSilQueue.pop(msg)) {}
        while (silToQtQueue.pop(msg)) {}
    }
private:
    QCanBusDevice* qtCanDevice{nullptr}; // onwer is the QTOBJECT
    //std::unique_ptr<SilKit::IParticipant> participant{nullptr}; // owner is CANBRIDGE CLASS UNIQUE
    SilKit::Services::Can::ICanController* silCanController; // owner is participant object
    std::shared_ptr<SilKit::IParticipant> participant; // NEW HUGO
    //std::vector<uint8_t> lastPayload;
    std::atomic<bool> running{false};
    std::thread silThread;
    //std::atomic<bool> silQueuePending{false};
    SafeQueue<CanMessage> qtToSilQueue;
    SafeQueue<CanMessage> silToQtQueue;
    using FrameKey = std::size_t;

    std::unordered_set<FrameKey> txCache;
    std::deque<std::pair<FrameKey, std::chrono::steady_clock::time_point>> txOrder;
    std::mutex txMutex;

    FrameKey makeFrameKey(const QCanBusFrame& frame)
    {
        std::size_t seed = 0;

        auto hashCombine = [&seed](auto v) {
            seed ^= std::hash<std::decay_t<decltype(v)>>{}(v)
                    + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };

        hashCombine(frame.frameId());

        QByteArray payload = frame.payload();
        for (char b : payload)
            hashCombine(static_cast<uint8_t>(b));

        return seed;
    }

    void cleanupTxCache()
    {
        using namespace std::chrono;

        auto now = steady_clock::now();

        while (!txOrder.empty())
        {
            auto [key, time] = txOrder.front();

            if (duration_cast<milliseconds>(now - time).count() > 200)
            {
                std::lock_guard<std::mutex> lock(txMutex);
                txCache.erase(key);
                txOrder.pop_front();
            }
            else
            {
                break;
            }
        }
    }

    void onQtFramesReceived()
    {
        CHECK_THREAD("onQtFramesReceived");
        std::cout << "Qt RX thread: "
          << QThread::currentThreadId() << std::endl;
        //Q_ASSERT(QThread::currentThread() == qtCanDevice->thread());

        while (qtCanDevice->framesAvailable()){

            QCanBusFrame frame = qtCanDevice->readFrame();

            FrameKey key = makeFrameKey(frame);

            {
                std::lock_guard<std::mutex> lock(txMutex);
    
                if (txCache.find(key) != txCache.end())
                {
                    // This is our own transmitted frame echo → ignore
                    continue;
                }
            }

            CanMessage msg;
            msg.id = frame.frameId();
            msg.data.assign(frame.payload().begin(), frame.payload().end());
            msg.source = Source::QtApp;
            qtToSilQueue.push(msg);


            cleanupTxCache();
            //std::vector<uint8_t> payload(frame.payload().begin(), frame.payload().end());
            /*lastPayload.assign(frame.payload().begin(), frame.payload().end());

            SilKit::Services::Can::CanFrame silFrame;
            silFrame.canId = frame.frameId();
            silFrame.dlc = static_cast<uint8_t>(lastPayload.size());
            silFrame.dataField = SilKit::Util::Span<const uint8_t>(lastPayload.data(), lastPayload.size());

            silCanController->SendFrame(silFrame);*/

            std::cout << "Qt -> SIL Kit: ID=" << frame.frameId() << std::endl;
        }

    }

    void onSilKitFrameReceived(const SilKit::Services::Can::CanFrameEvent& event)
    {
        /*if (qtCanDevice->thread() != QThread::currentThread()) {
            std::cerr << "THREAD MISMATCH DETECTED\n";
            std::terminate();
        }*/
        std::cout << "processSilQueue running in thread: "
        << QThread::currentThread() << std::endl;
        CanMessage msg;
        msg.id = event.frame.canId;
        msg.data.assign(
            event.frame.dataField.begin(),
            event.frame.dataField.end());
        
        msg.source = Source::SilKitBridge;
        silToQtQueue.push(msg);

        //if (!silQueuePending.exchange(true)) {
            QMetaObject::invokeMethod(this, [this, event](){
                onSilKitFrameReceived(event);
            }, Qt::QueuedConnection);
        //}
        /*QByteArray payload(
            reinterpret_cast<const char*>(event.frame.dataField.data()),
            static_cast<int>(event.frame.dlc));

        QCanBusFrame qtFrame(event.frame.canId, payload);
        qtCanDevice->writeFrame(qtFrame);*/

        std::cout << "SIL Kit -> Qt: ID=" << event.frame.canId << std::endl;
    }

    void silKitLoop()
    {
        std::cout << "SIL thread: "
          << std::this_thread::get_id() << std::endl;

        using namespace std::chrono;

        auto lastSend = steady_clock::now();
        while (running)
        {
            CanMessage msg;

            if (qtToSilQueue.pop(msg))
            {
                auto now = steady_clock::now();
                // throttle SIL send (VERY IMPORTANT)
                if (duration_cast<microseconds>(now - lastSend).count() < 500)
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(200));
                    continue;
                }
                static int qtToSilCount = 0;
                std::cout << "qt->sil: " << ++qtToSilCount << std::endl;

                SilKit::Services::Can::CanFrame frame;
                frame.canId = msg.id;
                frame.dlc = static_cast<uint8_t>(msg.data.size());
                frame.dataField = SilKit::Util::Span<const uint8_t>(
                    msg.data.data(),
                    msg.data.size());

                std::cout << "before SendFrame\n";
                silCanController->SendFrame(frame);
                std::cout << "after SendFrame\n";

                lastSend = now;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }

    void processSilQueue()
    {
        CanMessage msg;
        /*if (qtCanDevice->thread() != QThread::currentThread()) {
            std::cerr << "THREAD MISMATCH DETECTED\n";
            std::terminate();
        }*/
        std::cout << "processSilQueue running in thread: "
        << QThread::currentThread() << std::endl;
        while (silToQtQueue.pop(msg))
        {
            if (msg.source == Source::QtApp)
                continue; // 🔥 evita eco loop
            QByteArray payload(
                reinterpret_cast<const char*>(msg.data.data()),
                static_cast<int>(msg.data.size()));
    
            QCanBusFrame frame(msg.id, payload);
            if (qtCanDevice && qtCanDevice->state() == QCanBusDevice::ConnectedState)
            {
                FrameKey key = makeFrameKey(frame);

                {
                    std::lock_guard<std::mutex> lock(txMutex);
                    txCache.insert(key);
                    txOrder.emplace_back(key, std::chrono::steady_clock::now());
                }
                if (!qtCanDevice->writeFrame(frame)) {
                    std::cerr << "Qt CAN write failed\n";
                }
            }
        }

        //silQueuePending = false;

    }
};
